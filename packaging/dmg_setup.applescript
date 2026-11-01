-- Copyright (c) 2026 CrimsonSeraph(ltyy.leoyu@gmail.com)
-- SPDX-License-Identifier: GPL-3.0-only

-- dmg_setup.applescript
-- 用于在打包时配置 DMG 打开的 Finder 窗口视图、背景图及图标位置。

on run argv
    -- 延迟等待 Finder 完成挂载
    delay 2

    tell application "Finder"
        tell disk "DG-LAB Client"
            open
            set current view of container window to icon view
            set toolbar visible of container window to false
            set statusbar visible of container window to false
            -- 窗口大小边界 (左, 上, 右, 下)
            set the bounds of container window to {400, 200, 900, 500}

            set viewOptions to the icon view options of container window
            set arrangement of viewOptions to not arranged

            -- 图标大小
            set icon size of viewOptions to 100

            -- CPack 会自动将背景图放入 .background 文件夹
            set background picture of viewOptions to file ".background:background.png"

            -- 设置图标位置 (相对于窗口左上角)
            set position of item "DG-LAB-Client.app" of container window to {150, 250}
            set position of item "Applications" of container window to {450, 250}

            -- 隐藏背景文件夹，避免用户看到
            set the extension hidden of item ".background" of container window to true

            close
            open
            update without registering applications
            delay 1
        end tell
    end tell
end run
