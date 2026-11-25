#pragma once
#ifdef _WIN32
#include <QFile>
#include <QtEndian>
#include <windows.h>
#include <mmsystem.h>

// Per-player WinMM device: avoids Qt Multimedia's Wine startup stall and
// MCI waveaudio's missing volume command. The PCM buffer outlives playback.
class WineAudio {
    HWAVEOUT device = nullptr;
    WAVEHDR header{};
    QByteArray samples;
    WAVEFORMATEX format{};
    DWORD baseBytes = 0, pausedBytes = 0;
    bool queued = false, paused = false;
    unsigned volume = 800;
    bool check(MMRESULT result) { return result == MMSYSERR_NOERROR; }
    DWORD positionBytes() const {
        if (!device || !queued) return baseBytes;
        if (paused) return pausedBytes;
        MMTIME time{}; time.wType = TIME_BYTES;
        if (waveOutGetPosition(device, &time, sizeof(time))) return baseBytes;
        DWORD offset = time.wType == TIME_BYTES ? time.u.cb :
            time.wType == TIME_SAMPLES ? time.u.sample * format.nBlockAlign :
            time.wType == TIME_MS ? (uint64_t(time.u.ms) * format.nAvgBytesPerSec / 1000) : 0;
        return std::min<DWORD>(samples.size(), baseBytes + offset);
    }
    void reset() {
        if (!device) return;
        waveOutReset(device);
        if (queued) waveOutUnprepareHeader(device, &header, sizeof(header));
        header = {}; queued = false; paused = false;
    }
    bool play() {
        if (!device) return false;
        if (queued && paused) { paused = false; return check(waveOutRestart(device)); }
        if (queued && !(header.dwFlags & WHDR_DONE)) return true;
        reset();
        if (baseBytes >= DWORD(samples.size())) baseBytes = 0;
        header.lpData = samples.data() + baseBytes;
        header.dwBufferLength = samples.size() - baseBytes;
        if (!check(waveOutPrepareHeader(device, &header, sizeof(header)))) return false;
        queued = true;
        return check(waveOutWrite(device, &header, sizeof(header)));
    }
public:
    ~WineAudio() { close(); }
    void close() { reset(); if (device) waveOutClose(device); device = nullptr; samples.clear(); baseBytes = 0; }
    bool open(const QString& path) {
        close(); QFile file(path); if (!file.open(QIODevice::ReadOnly)) return false;
        const QByteArray bytes = file.readAll();
        if (bytes.size() < 12 || bytes.left(4) != "RIFF" || bytes.mid(8,4) != "WAVE") return false;
        bool foundFormat = false, foundData = false;
        for (qsizetype pos = 12; pos + 8 <= bytes.size();) {
            const quint32 length = qFromLittleEndian<quint32>(bytes.constData() + pos + 4);
            const qsizetype start = pos + 8;
            if (length > quint64(bytes.size() - start)) return false;
            const QByteArray tag = bytes.mid(pos,4);
            if (tag == "fmt " && length >= 16) {
                format = {};
                format.wFormatTag = qFromLittleEndian<quint16>(bytes.constData()+start);
                format.nChannels = qFromLittleEndian<quint16>(bytes.constData()+start+2);
                format.nSamplesPerSec = qFromLittleEndian<quint32>(bytes.constData()+start+4);
                format.nAvgBytesPerSec = qFromLittleEndian<quint32>(bytes.constData()+start+8);
                format.nBlockAlign = qFromLittleEndian<quint16>(bytes.constData()+start+12);
                format.wBitsPerSample = qFromLittleEndian<quint16>(bytes.constData()+start+14);
                foundFormat = true;
            } else if (tag == "data") { samples = bytes.mid(start,length); foundData = true; }
            pos = start + length + (length & 1);
        }
        if (!foundFormat || !foundData || samples.isEmpty() || format.wFormatTag != WAVE_FORMAT_PCM ||
            format.wBitsPerSample != 16 || (format.nChannels != 1 && format.nChannels != 2) ||
            !format.nSamplesPerSec || format.nBlockAlign != format.nChannels * 2 ||
            format.nAvgBytesPerSec != format.nSamplesPerSec * format.nBlockAlign ||
            samples.size() % format.nBlockAlign) return false;
        if (!check(waveOutOpen(&device, WAVE_MAPPER, &format, 0, 0, CALLBACK_NULL))) { device = nullptr; return false; }
        return setVolume(volume);
    }
    bool setVolume(unsigned value) { volume = std::min(1000u,value); const DWORD level = volume * 65535u / 1000u;
        return device && check(waveOutSetVolume(device, level | (level << 16))); }
    QString query(const QString& property) const {
        if (!device) return {};
        if (property == "length") return QString::number(uint64_t(samples.size()) * 1000 / format.nAvgBytesPerSec);
        if (property == "position") return QString::number(uint64_t(positionBytes()) * 1000 / format.nAvgBytesPerSec);
        if (property == "mode") return queued && !(header.dwFlags & WHDR_DONE) ? (paused ? "paused" : "playing") : "stopped";
        return {};
    }
    bool command(const QString& command) {
        if (command.startsWith("close ")) { close(); return true; }
        if (!device) return false;
        if (command.startsWith("play ") || command.startsWith("resume ")) return play();
        if (command.startsWith("pause ")) { const DWORD position = positionBytes(); if (!check(waveOutPause(device))) return false; pausedBytes = position; paused = true; return true; }
        if (command.startsWith("stop ")) { const DWORD position = positionBytes(); reset(); baseBytes = position; return true; }
        if (command.startsWith("seek ")) {
            const QString target = command.section(" to ",1);
            const quint64 ms = target == "start" ? 0 : target.toULongLong();
            reset(); baseBytes = std::min<quint64>(samples.size(), ms * format.nAvgBytesPerSec / 1000);
            baseBytes -= baseBytes % format.nBlockAlign; return true;
        }
        if (command.startsWith("setaudio ")) return setVolume(command.section(" to ",1).toUInt());
        return false;
    }
};
#endif
