cmake_minimum_required(VERSION 3.22)
project(IGIQtCocoaFix LANGUAGES CXX OBJCXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_AUTOMOC ON)
set(CMAKE_OSX_DEPLOYMENT_TARGET 12.0)
find_package(Qt6 REQUIRED COMPONENTS Core Gui CorePrivate GuiPrivate)
if(NOT EXISTS "${QT_COCOA_SOURCE}/qcocoaaccessibilityelement.mm")
  message(FATAL_ERROR "Set QT_COCOA_SOURCE to the matching patched Qt Cocoa source directory")
endif()
add_library(qcocoa MODULE
  ${QT_COCOA_SOURCE}/main.mm
  ${QT_COCOA_SOURCE}/qcocoaapplication.h
  ${QT_COCOA_SOURCE}/qcocoaapplication.mm
  ${QT_COCOA_SOURCE}/qcocoaapplicationdelegate.h
  ${QT_COCOA_SOURCE}/qcocoaapplicationdelegate.mm
  ${QT_COCOA_SOURCE}/qcocoabackingstore.h
  ${QT_COCOA_SOURCE}/qcocoabackingstore.mm
  ${QT_COCOA_SOURCE}/qcocoaclipboard.h
  ${QT_COCOA_SOURCE}/qcocoaclipboard.mm
  ${QT_COCOA_SOURCE}/qcocoacursor.h
  ${QT_COCOA_SOURCE}/qcocoacursor.mm
  ${QT_COCOA_SOURCE}/qcocoadrag.h
  ${QT_COCOA_SOURCE}/qcocoadrag.mm
  ${QT_COCOA_SOURCE}/qcocoaeventdispatcher.h
  ${QT_COCOA_SOURCE}/qcocoaeventdispatcher.mm
  ${QT_COCOA_SOURCE}/qcocoahelpers.h
  ${QT_COCOA_SOURCE}/qcocoahelpers.mm
  ${QT_COCOA_SOURCE}/qcocoainputcontext.h
  ${QT_COCOA_SOURCE}/qcocoainputcontext.mm
  ${QT_COCOA_SOURCE}/qcocoaintegration.h
  ${QT_COCOA_SOURCE}/qcocoaintegration.mm
  ${QT_COCOA_SOURCE}/qcocoaintrospection.h
  ${QT_COCOA_SOURCE}/qcocoaintrospection.mm
  ${QT_COCOA_SOURCE}/qcocoamenu.h
  ${QT_COCOA_SOURCE}/qcocoamenu.mm
  ${QT_COCOA_SOURCE}/qcocoamenubar.h
  ${QT_COCOA_SOURCE}/qcocoamenubar.mm
  ${QT_COCOA_SOURCE}/qcocoamenuitem.h
  ${QT_COCOA_SOURCE}/qcocoamenuitem.mm
  ${QT_COCOA_SOURCE}/qcocoamenuloader.h
  ${QT_COCOA_SOURCE}/qcocoamenuloader.mm
  ${QT_COCOA_SOURCE}/qcocoamimetypes.h
  ${QT_COCOA_SOURCE}/qcocoamimetypes.mm
  ${QT_COCOA_SOURCE}/qcocoanativeinterface.h
  ${QT_COCOA_SOURCE}/qcocoanativeinterface.mm
  ${QT_COCOA_SOURCE}/qcocoansmenu.h
  ${QT_COCOA_SOURCE}/qcocoansmenu.mm
  ${QT_COCOA_SOURCE}/qcocoascreen.h
  ${QT_COCOA_SOURCE}/qcocoascreen.mm
  ${QT_COCOA_SOURCE}/qcocoaservices.h
  ${QT_COCOA_SOURCE}/qcocoaservices.mm
  ${QT_COCOA_SOURCE}/qcocoasystemtrayicon.h
  ${QT_COCOA_SOURCE}/qcocoasystemtrayicon.mm
  ${QT_COCOA_SOURCE}/qcocoatheme.h
  ${QT_COCOA_SOURCE}/qcocoatheme.mm
  ${QT_COCOA_SOURCE}/qcocoawindow.h
  ${QT_COCOA_SOURCE}/qcocoawindow.mm
  ${QT_COCOA_SOURCE}/qcocoawindowmanager.h
  ${QT_COCOA_SOURCE}/qcocoawindowmanager.mm
  ${QT_COCOA_SOURCE}/qiosurfacegraphicsbuffer.h
  ${QT_COCOA_SOURCE}/qiosurfacegraphicsbuffer.mm
  ${QT_COCOA_SOURCE}/qmacclipboard.h
  ${QT_COCOA_SOURCE}/qmacclipboard.mm
  ${QT_COCOA_SOURCE}/qmultitouch_mac.mm
  ${QT_COCOA_SOURCE}/qmultitouch_mac_p.h
  ${QT_COCOA_SOURCE}/qnsview.h
  ${QT_COCOA_SOURCE}/qnsview.mm
  ${QT_COCOA_SOURCE}/qnswindow.h
  ${QT_COCOA_SOURCE}/qnswindow.mm
  ${QT_COCOA_SOURCE}/qnswindowdelegate.h
  ${QT_COCOA_SOURCE}/qnswindowdelegate.mm
  ${QT_COCOA_SOURCE}/qcocoacolordialoghelper.h
  ${QT_COCOA_SOURCE}/qcocoacolordialoghelper.mm
  ${QT_COCOA_SOURCE}/qcocoafiledialoghelper.h
  ${QT_COCOA_SOURCE}/qcocoafiledialoghelper.mm
  ${QT_COCOA_SOURCE}/qcocoafontdialoghelper.h
  ${QT_COCOA_SOURCE}/qcocoafontdialoghelper.mm
  ${QT_COCOA_SOURCE}/qcocoamessagedialog.h
  ${QT_COCOA_SOURCE}/qcocoamessagedialog.mm
  ${QT_COCOA_SOURCE}/qcocoaglcontext.mm
  ${QT_COCOA_SOURCE}/qcocoaglcontext.h
  ${QT_COCOA_SOURCE}/qcocoavulkaninstance.mm
  ${QT_COCOA_SOURCE}/qcocoavulkaninstance.h
  ${QT_COCOA_SOURCE}/qcocoaaccessibility.mm
  ${QT_COCOA_SOURCE}/qcocoaaccessibility.h
  ${QT_COCOA_SOURCE}/qcocoaaccessibilityelement.mm
  ${QT_COCOA_SOURCE}/qcocoaaccessibilityelement.h
  ${QT_COCOA_SOURCE}/qcocoasessionmanager.cpp
  ${QT_COCOA_SOURCE}/qcocoasessionmanager.h
)
target_link_libraries(qcocoa PRIVATE Qt6::Core Qt6::CorePrivate Qt6::Gui Qt6::GuiPrivate)
target_compile_definitions(qcocoa PRIVATE QT_PLUGIN QT_NO_DEBUG)
find_library(FW_Foundation Foundation REQUIRED)
target_link_libraries(qcocoa PRIVATE ${FW_Foundation})
find_library(FW_AppKit AppKit REQUIRED)
target_link_libraries(qcocoa PRIVATE ${FW_AppKit})
find_library(FW_Carbon Carbon REQUIRED)
target_link_libraries(qcocoa PRIVATE ${FW_Carbon})
find_library(FW_CoreServices CoreServices REQUIRED)
target_link_libraries(qcocoa PRIVATE ${FW_CoreServices})
find_library(FW_CoreVideo CoreVideo REQUIRED)
target_link_libraries(qcocoa PRIVATE ${FW_CoreVideo})
find_library(FW_IOKit IOKit REQUIRED)
target_link_libraries(qcocoa PRIVATE ${FW_IOKit})
find_library(FW_IOSurface IOSurface REQUIRED)
target_link_libraries(qcocoa PRIVATE ${FW_IOSurface})
find_library(FW_Metal Metal REQUIRED)
target_link_libraries(qcocoa PRIVATE ${FW_Metal})
find_library(FW_QuartzCore QuartzCore REQUIRED)
target_link_libraries(qcocoa PRIVATE ${FW_QuartzCore})
find_library(FW_UniformTypeIdentifiers UniformTypeIdentifiers REQUIRED)
target_link_libraries(qcocoa PRIVATE ${FW_UniformTypeIdentifiers})
qt_add_resources(qcocoa qcocoaresources PREFIX "/qt-project.org/mac/cursors" BASE "${QT_COCOA_SOURCE}" FILES "${QT_COCOA_SOURCE}/images/sizeallcursor.png" "${QT_COCOA_SOURCE}/images/spincursor.png" "${QT_COCOA_SOURCE}/images/waitcursor.png")

target_include_directories(qcocoa PRIVATE "${VULKAN_HEADERS}")

target_include_directories(qcocoa PRIVATE "${MOLTENVK_HEADERS}")
set_target_properties(qcocoa PROPERTIES SUFFIX ".dylib")
