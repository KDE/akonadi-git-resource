project(akonadi_git_resource)

find_package(Akonadi)
include_directories( ${Boost_INCLUDE_DIR} )
set(RESOURCE_VERSION "0.1")
set(LIB_VERSION "1.0")
set(LIB_SOVERSION "1")

set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake/modules")
find_package(KDE4 REQUIRED)
include(KDE4Defaults)
include(MacroLibrary)
find_package(KdepimLibs 4.5.63 REQUIRED)

add_definitions(${QT_DEFINITIONS} ${KDE4_DEFINITIONS})
add_definitions (-DQT_USE_FAST_CONCATENATION -DQT_USE_FAST_OPERATOR_PLUS)

set( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${KDE4_ENABLE_EXCEPTIONS}" )

set( gitresource_SRCS gitresource.cpp configdialog.cpp gitthread.cpp flagdatabase.cpp )

include_directories(
  ${CMAKE_SOURCE_DIR}
  ${CMAKE_BINARY_DIR}
  ${KDEPIMLIBS_INCLUDE_DIRS}
  ${KDE4_INCLUDES}
  ${Boost_INCLUDE_DIR}
)
kde4_add_ui_files(gitresource_SRCS configdialog.ui)
kde4_add_kcfg_files(gitresource_SRCS settings.kcfgc)
find_program(XSLTPROC_EXECUTABLE xsltproc)
macro(kcfg_generate_dbus_interface _kcfg _name)
  add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${_name}.xml
    COMMAND ${XSLTPROC_EXECUTABLE} --stringparam interfaceName ${_name}
    ${KDEPIMLIBS_DATA_DIR}/akonadi-kde/kcfg2dbus.xsl
    ${_kcfg}
    > ${CMAKE_CURRENT_BINARY_DIR}/${_name}.xml
    DEPENDS ${KDEPIMLIBS_DATA_DIR}/akonadi-kde/kcfg2dbus.xsl
    ${_kcfg}
    )
endmacro(kcfg_generate_dbus_interface)

kcfg_generate_dbus_interface(${CMAKE_CURRENT_SOURCE_DIR}/gitresource.kcfg org.kde.Akonadi.Git.Settings)

qt4_add_dbus_adaptor(gitresource_SRCS
  ${CMAKE_CURRENT_BINARY_DIR}/org.kde.Akonadi.Git.Settings.xml settings.h GitSettings
)

kde4_add_plugin(akonadi_git_resource ${gitresource_SRCS})

if (Q_WS_MAC)
  set_target_properties(akonadi_git_resource PROPERTIES MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/../Info.plist.template)
  set_target_properties(akonadi_git_resource PROPERTIES MACOSX_BUNDLE_GUI_IDENTIFIER "org.kde.Akonadi.Git")
  set_target_properties(akonadi_git_resource PROPERTIES MACOSX_BUNDLE_BUNDLE_NAME "KDE Akonadi Git Resource")
endif (Q_WS_MAC)

target_link_libraries(akonadi_git_resource
  ${KDEPIMLIBS_AKONADI_LIBS}
  ${KDEPIMLIBS_KCALCORE_LIBS}
  ${KDEPIMLIBS_KMIME_LIBS}
  ${KDE4_KIO_LIBS}
  git2
  QtSql
)

install( TARGETS akonadi_git_resource DESTINATION ${PLUGIN_INSTALL_DIR}/ )
install( FILES gitresource.desktop DESTINATION "${CMAKE_INSTALL_PREFIX}/share/akonadi/agents" )

