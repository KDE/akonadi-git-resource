project(akonadi_git_resource)

cmake_minimum_required(VERSION 2.8.8)
include(FeatureSummary)

set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake/modules")

find_package(Libgit2 0.17.0)
set_package_properties(Libgit2 PROPERTIES DESCRIPTION "LibGit library" URL "http://libgit2.github.com/" TYPE REQUIRED)

find_package(Akonadi QUIET CONFIG)
set_package_properties(Akonadi PROPERTIES DESCRIPTION "Akonadi server libraries" URL "http://pim.kde.org/akonadi" TYPE REQUIRED PURPOSE "Akonadi is required to build the GIT resource")

find_package(KDE4 4.9.4 REQUIRED)
include(KDE4Defaults)

find_package(KdepimLibs 4.8.0)
set_package_properties(KdepimLibs PROPERTIES DESCRIPTION "The KDEPIM libraries" URL "http://www.kde.org" TYPE REQUIRED)

find_package(Xsltproc)
set_package_properties(Xsltproc PROPERTIES DESCRIPTION "XSLT processor from libxslt" TYPE REQUIRED PURPOSE "Required to generate D-Bus interfaces.")

feature_summary(WHAT ALL FATAL_ON_MISSING_REQUIRED_PACKAGES)

set(RESOURCE_VERSION "0.1")
set(LIB_VERSION "1.0")
set(LIB_SOVERSION "1")
set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake/modules")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${KDE4_ENABLE_EXCEPTIONS}" )
set(gitresource_SRCS cheatingutils.cpp
                     configdialog.cpp
                     flagdatabase.cpp
                     gitresource.cpp
                     gitthread.cpp )

add_definitions(${QT_DEFINITIONS}
                ${KDE4_DEFINITIONS}
                -DQT_USE_FAST_CONCATENATION
                -DQT_USE_FAST_OPERATOR_PLUS)


include_directories(
  ${CMAKE_SOURCE_DIR}
  ${CMAKE_BINARY_DIR}
  ${KDEPIMLIBS_INCLUDE_DIRS}
  ${KDE4_INCLUDES}
  ${Boost_INCLUDE_DIR}
  ${Libgit2_INCLUDE_DIR}
)

kde4_add_ui_files(gitresource_SRCS configdialog.ui)
kde4_add_kcfg_files(gitresource_SRCS settings.kcfgc)

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
  ${KDEPIMLIBS_KPIMIDENTITIES_LIBS}
  ${QT_QTSQL_LIBRARY}
  # ${Libgit2_LIBRARY}
  git2
)
install( TARGETS akonadi_git_resource DESTINATION ${PLUGIN_INSTALL_DIR}/ )
install( FILES gitresource.desktop DESTINATION "${CMAKE_INSTALL_PREFIX}/share/akonadi/agents" )
