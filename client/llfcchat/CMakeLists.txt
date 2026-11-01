cmake_minimum_required(VERSION 3.19)
project(llfcchat LANGUAGES CXX)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Widgets Network)

qt_standard_project_setup()
set (CMAKE_AUTORCC ON)
if(MINGW)
    set(EXE_WIN32 "")
else()
    set(EXE_WIN32 WIN32)
endif()

qt_add_executable(llfcchat
    ${EXE_WIN32} MACOSX_BUNDLE
    main.cpp
    mainwindow.cpp
    mainwindow.h
    mainwindow.ui
    rc.qrc
    logindialog.h logindialog.cpp logindialog.ui
    registerdialog.h registerdialog.cpp registerdialog.ui
    global.h
    global.cpp
    singleton.h
    httpmgr.h httpmgr.cpp
    timerbtn.h timerbtn.cpp
    clickedlabel.h clickedlabel.cpp
    resetdialog.h resetdialog.cpp resetdialog.ui
    tcpmgr.h tcpmgr.cpp
    usermgr.h usermgr.cpp
    chatdialog.h chatdialog.cpp chatdialog.ui
    clickedbtn.h clickedbtn.cpp
    customizeedit.h customizeedit.cpp
    chatuserlist.h chatuserlist.cpp
    chatuserwid.h chatuserwid.cpp chatuserwid.ui
    listitembase.h listitembase.cpp
    loadingdialog.h loadingdialog.cpp loadingdialog.ui
    chatpage.h chatpage.cpp chatpage.ui
    chatview.h chatview.cpp
    chatitembase.h chatitembase.cpp
    bubbleframe.h bubbleframe.cpp
    textbubble.h textbubble.cpp
    picturebubble.h picturebubble.cpp
    messagetextedit.h messagetextedit.cpp
    statewidget.h statewidget.cpp
    searchlist.h searchlist.cpp
    userdata.h userdata.cpp
    adduseritem.h adduseritem.cpp adduseritem.ui
    findsuccessdlg.h findsuccessdlg.cpp findsuccessdlg.ui
    applyfriend.h applyfriend.cpp applyfriend.ui
    clickedoncelabel.h clickedoncelabel.cpp
    friendlabel.h friendlabel.cpp friendlabel.ui
    contactuserlist.h contactuserlist.cpp
    conuseritem.h conuseritem.cpp conuseritem.ui
    grouptipitem.h grouptipitem.cpp grouptipitem.ui
    applyfriendpage.h applyfriendpage.cpp applyfriendpage.ui
    applyfrienditem.h applyfrienditem.cpp applyfrienditem.ui
    applyfriendlist.h applyfriendlist.cpp
    findfaildlg.h findfaildlg.cpp findfaildlg.ui
    logger.h logger.cpp
    authenfriend.h authenfriend.cpp authenfriend.ui
    findhasfrienddlg.h findhasfrienddlg.cpp findhasfrienddlg.ui



)

target_link_libraries(llfcchat
    PRIVATE
        Qt::Core
        Qt::Widgets
        Qt::Network
)

target_include_directories(llfcchat PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# ========== 构建后自动拷贝 config.ini ==========
if(EXISTS "${CMAKE_SOURCE_DIR}/config.ini")
    add_custom_command(TARGET llfcchat POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_SOURCE_DIR}/config.ini"
            "$<TARGET_FILE_DIR:llfcchat>/"
        COMMENT "Copying config.ini to output directory..."
    )
else()
    message(WARNING "未找到 config.ini，已跳过复制；请复制 config.ini.example 为 config.ini 后重新构建。")
endif()

# ========== 构建后自动拷贝 static 目录（头像等静态资源）==========
add_custom_command(TARGET llfcchat POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/static"
        "$<TARGET_FILE_DIR:llfcchat>/static"
    COMMENT "Copying static directory to output directory..."
)

# ========== 新增：安装时也包含 config.ini ==========
install(TARGETS llfcchat
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

# 安装配置文件到 bin 目录
if(EXISTS "${CMAKE_SOURCE_DIR}/config.ini")
    install(FILES "${CMAKE_SOURCE_DIR}/config.ini"
        DESTINATION ${CMAKE_INSTALL_BINDIR}
    )
endif()

qt_generate_deploy_app_script(
    TARGET llfcchat
    OUTPUT_SCRIPT deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR
)
# install(TARGETS llfcchat
#     BUNDLE  DESTINATION .
#     RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
#     LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
# )

# qt_generate_deploy_app_script(
#     TARGET llfcchat
#     TARGET llfcchat
#     OUTPUT_SCRIPT deploy_script
#     NO_UNSUPPORTED_PLATFORM_ERROR
# )
install(SCRIPT ${deploy_script})
