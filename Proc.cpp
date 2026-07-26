#include <Windows.h>
#include <string>
#include <vector>
#include "resource.h"
#include "common.h"
#include "FileOperation.h"
#include "SOO_Ex.h"
#include "Time.h"

static std::vector<std::wstring> g_backupList;   // 备份时间戳列表（供选择对话框使用）
static std::wstring g_selectedBackup;            // 用户选中的时间戳
static std::wstring g_defaultFolder;   // 用于在向导对话框间传递用户选择的文件夹路径


// 新建向导的对话框过程
INT_PTR CALLBACK WizardProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_INITDIALOG: {
        // 窗口居中于屏幕工作区
        RECT rcDlg;
        GetWindowRect(hDlg, &rcDlg);
        RECT rcScreen;
        SystemParametersInfoW(SPI_GETWORKAREA, 0, &rcScreen, 0);
        int x = rcScreen.left + (rcScreen.right - rcScreen.left - (rcDlg.right - rcDlg.left)) / 2;
        int y = rcScreen.top + (rcScreen.bottom - rcScreen.top - (rcDlg.bottom - rcDlg.top)) / 2;
        SetWindowPos(hDlg, NULL, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);

        // 设置对话框图标
        HINSTANCE hInst = (HINSTANCE)GetWindowLongPtrW(hDlg, GWLP_HINSTANCE);
        HICON hIcon = LoadIconW(hInst, MAKEINTRESOURCEW(IDI_MAIN_ICON));
        SendMessageW(hDlg, WM_SETICON, ICON_SMALL, (LPARAM)hIcon);

        // 保存用户右键新建时所在的文件夹（从 lParam 传入）
        if (lParam) {
            g_defaultFolder = (wchar_t*)lParam;
            size_t pos = g_defaultFolder.find_last_of(L"\\/");
            if (pos != std::wstring::npos) g_defaultFolder = g_defaultFolder.substr(0, pos + 1);
        }
        // 默认选中“托管模式”
        CheckRadioButton(hDlg, IDC_MODE_MANAGED, IDC_MODE_FREE, IDC_MODE_MANAGED);
        // 设置冷却时间编辑框的默认值
        SetDlgItemInt(hDlg, IDC_COOLDOWN_EDIT, COOLDOWN, FALSE);
        //版本号和著作权信息
        wchar_t text_IDC_VERSION_COPYRIGHT[64];
        swprintf_s(text_IDC_VERSION_COPYRIGHT, 64, L"StartOnlyOne Version:%ls    Copyright (C) %ls", VERSION, COPYRIGHT);
        SetDlgItemTextW(hDlg, IDC_VERSION_COPYRIGHT, text_IDC_VERSION_COPYRIGHT);
        return TRUE;
    }
    case WM_COMMAND: {
        switch (LOWORD(wParam)) {
        case IDC_BROWSE_BTN: {   // 浏览按钮：选择目标 exe
            OPENFILENAMEW ofn = { 0 };
            wchar_t filePath[MAX_PATH] = { 0 };
            ofn.lStructSize = sizeof(ofn);
            ofn.hwndOwner = hDlg;
            ofn.lpstrFilter = L"可执行文件\0*.exe\0所有文件\0*.*\0";
            ofn.lpstrFile = filePath;
            ofn.nMaxFile = MAX_PATH;
            ofn.Flags = OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;
            if (GetOpenFileNameW(&ofn)) {
                SetDlgItemTextW(hDlg, IDC_TARGET_EDIT, filePath);
                // 自动将文件名（不含扩展名）填入“目标文件名”编辑框
                std::wstring defaultName = GetFileNameWithoutExt(filePath);
                SetDlgItemTextW(hDlg, IDC_NAME_EDIT, defaultName.c_str());
            }
            return TRUE;
        }
        case IDOK: {
            // 获取用户输入
            wchar_t targetExe[MAX_PATH] = { 0 };
            GetDlgItemTextW(hDlg, IDC_TARGET_EDIT, targetExe, MAX_PATH);
            if (!targetExe[0]) {
                MessageBoxW(hDlg, L"请选择程序！", L"提示", MB_ICONWARNING);
                return TRUE;
            }

            wchar_t fileName[MAX_PATH] = { 0 };
            GetDlgItemTextW(hDlg, IDC_NAME_EDIT, fileName, MAX_PATH);
            if (!fileName[0]) wcscpy_s(fileName, GetFileNameWithoutExt(targetExe).c_str());

            wchar_t args[1024] = { 0 };
            GetDlgItemTextW(hDlg, IDC_ARGS_EDIT, args, 1024);
            BOOL overwrite = IsDlgButtonChecked(hDlg, IDC_OVERWRITE_CHECK) == BST_CHECKED;
            BOOL isManaged = IsDlgButtonChecked(hDlg, IDC_MODE_MANAGED) == BST_CHECKED;
            int cooldown = max(0, GetDlgItemInt(hDlg, IDC_COOLDOWN_EDIT, NULL, FALSE));

            bool success = isManaged ? CreateManaged(targetExe, args, fileName, cooldown,
                g_defaultFolder, overwrite, hDlg, targetExe, 0)
                : CreateFree(targetExe, args, fileName, cooldown, g_defaultFolder, overwrite, hDlg);
            if (success) {
                MessageBoxW(hDlg, L"创建成功！", L"完成", MB_OK);
                EndDialog(hDlg, IDOK);
            }
            return TRUE;
        }
        case IDCANCEL:
            EndDialog(hDlg, IDCANCEL);
            return TRUE;
        }
        break;
    }
    }
    return FALSE;
}

// 备份选择对话框过程
INT_PTR CALLBACK SelectBackupProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_INITDIALOG: {
        // 居中窗口
        RECT rcDlg, rcScreen;
        GetWindowRect(hDlg, &rcDlg);
        SystemParametersInfoW(SPI_GETWORKAREA, 0, &rcScreen, 0);
        int x = rcScreen.left + (rcScreen.right - rcScreen.left - (rcDlg.right - rcDlg.left)) / 2;
        int y = rcScreen.top + (rcScreen.bottom - rcScreen.top - (rcDlg.bottom - rcDlg.top)) / 2;
        SetWindowPos(hDlg, NULL, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);

        // 设置图标
        HINSTANCE hInst = (HINSTANCE)GetWindowLongPtrW(hDlg, GWLP_HINSTANCE);
        HICON hIcon = LoadIconW(hInst, MAKEINTRESOURCEW(IDI_MAIN_ICON));
        SendMessageW(hDlg, WM_SETICON, ICON_SMALL, (LPARAM)hIcon);

        // 接收备份列表并显示在 ListView 中
        g_backupList = *(std::vector<std::wstring>*)lParam;
        HWND hList = GetDlgItem(hDlg, IDC_BACKUP_LIST);
        ListView_SetExtendedListViewStyle(hList, LVS_EX_FULLROWSELECT);
        LVCOLUMNW col = { 0 };
        col.mask = LVCF_TEXT | LVCF_WIDTH;
        col.pszText = const_cast<LPWSTR>(L"备份时间");
        col.cx = 250;
        ListView_InsertColumn(hList, 0, &col);

        for (int i = 0; i < g_backupList.size(); i++) {
            std::wstring display = TimestampToDisplay(g_backupList[i]);
            LVITEMW item = { 0 };
            item.mask = LVIF_TEXT;
            item.iItem = i;
            item.pszText = (wchar_t*)display.c_str();
            ListView_InsertItem(hList, &item);
        }
        return TRUE;
    }
    case WM_COMMAND: {
        if (LOWORD(wParam) == IDC_BTN_RESTORE_SELECTED) {
            HWND hList = GetDlgItem(hDlg, IDC_BACKUP_LIST);
            int sel = ListView_GetNextItem(hList, -1, LVNI_SELECTED);
            if (sel == -1) { MessageBoxW(hDlg, L"请选择备份！", L"提示", MB_OK); return TRUE; }
            wchar_t buf[256];
            ListView_GetItemText(hList, sel, 0, buf, 256);
            std::wstring display = buf;
            // 根据显示文本查找对应的时间戳
            for (auto& ts : g_backupList) {
                if (TimestampToDisplay(ts) == display) {
                    g_selectedBackup = ts;
                    EndDialog(hDlg, IDOK);
                    return TRUE;
                }
            }
        }
        else if (LOWORD(wParam) == IDCANCEL) EndDialog(hDlg, IDCANCEL);
        break;
    }
    }
    return FALSE;
}

// 转换桌面快捷方式引导对话框（允许用户输入冷却时间）
INT_PTR CALLBACK ConvertGuideProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg)
    {
    case WM_INITDIALOG: {
        // 居中
        RECT rcDlg, rcScreen;
        GetWindowRect(hDlg, &rcDlg);
        SystemParametersInfoW(SPI_GETWORKAREA, 0, &rcScreen, 0);
        int x = rcScreen.left + (rcScreen.right - rcScreen.left - (rcDlg.right - rcDlg.left)) / 2;
        int y = rcScreen.top + (rcScreen.bottom - rcScreen.top - (rcDlg.bottom - rcDlg.top)) / 2;
        SetWindowPos(hDlg, NULL, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
        SetDlgItemInt(hDlg, IDC_COOLDOWN_EDIT, COOLDOWN, FALSE);
        // 设置图标
        HINSTANCE hInst = (HINSTANCE)GetWindowLongPtrW(hDlg, GWLP_HINSTANCE);
        HICON hIcon = LoadIconW(hInst, MAKEINTRESOURCEW(IDI_MAIN_ICON));
        SendMessageW(hDlg, WM_SETICON, ICON_SMALL, (LPARAM)hIcon);
        return TRUE;
    }
    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDCANCEL:
            EndDialog(hDlg, IDCANCEL);
            return FALSE;
        case IDOK: {
            int cooldown = max(0, GetDlgItemInt(hDlg, IDC_COOLDOWN_EDIT, NULL, FALSE));
            ConvertDesktopAllLnk(hDlg, cooldown);
            EndDialog(hDlg, IDCANCEL);
            return TRUE;
        }
        }
        return FALSE;
    case WM_CLOSE:
        EndDialog(hDlg, IDCANCEL);
        return FALSE;
    }
    return FALSE;
}

// 备份并转换桌面快捷方式引导对话框
INT_PTR CALLBACK BackupConvertGuideProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg)
    {
    case WM_INITDIALOG: {
        // 居中
        RECT rcDlg, rcScreen;
        GetWindowRect(hDlg, &rcDlg);
        SystemParametersInfoW(SPI_GETWORKAREA, 0, &rcScreen, 0);
        int x = rcScreen.left + (rcScreen.right - rcScreen.left - (rcDlg.right - rcDlg.left)) / 2;
        int y = rcScreen.top + (rcScreen.bottom - rcScreen.top - (rcDlg.bottom - rcDlg.top)) / 2;
        SetWindowPos(hDlg, NULL, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
        SetDlgItemInt(hDlg, IDC_COOLDOWN_EDIT, COOLDOWN, FALSE);
        // 设置图标
        HINSTANCE hInst = (HINSTANCE)GetWindowLongPtrW(hDlg, GWLP_HINSTANCE);
        HICON hIcon = LoadIconW(hInst, MAKEINTRESOURCEW(IDI_MAIN_ICON));
        SendMessageW(hDlg, WM_SETICON, ICON_SMALL, (LPARAM)hIcon);
        return TRUE;
    }
    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDCANCEL:
            EndDialog(hDlg, IDCANCEL);
            return FALSE;
        case IDOK: {
            int cooldown = max(0, GetDlgItemInt(hDlg, IDC_COOLDOWN_EDIT, NULL, FALSE));
            BackupDesktopAllLnk(hDlg);
            ConvertDesktopAllLnk(hDlg, cooldown);
            EndDialog(hDlg, IDCANCEL);
            return TRUE;
        }
        }
        return FALSE;
    case WM_CLOSE:
        EndDialog(hDlg, IDCANCEL);
        return FALSE;
    }
    return FALSE;
}

//关于对话框过程
INT_PTR CALLBACK AboutProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_INITDIALOG: {
        // 居中窗口
        RECT rcDlg, rcScreen;
        GetWindowRect(hDlg, &rcDlg);
        SystemParametersInfoW(SPI_GETWORKAREA, 0, &rcScreen, 0);
        int x = rcScreen.left + (rcScreen.right - rcScreen.left - (rcDlg.right - rcDlg.left)) / 2;
        int y = rcScreen.top + (rcScreen.bottom - rcScreen.top - (rcDlg.bottom - rcDlg.top)) / 2;
        SetWindowPos(hDlg, NULL, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);

        // 设置窗口图标
        HINSTANCE hInst = (HINSTANCE)GetWindowLongPtrW(hDlg, GWLP_HINSTANCE);
        HICON hIcon = LoadIconW(hInst, MAKEINTRESOURCEW(IDI_MAIN_ICON));
        SendMessageW(hDlg, WM_SETICON, ICON_SMALL, (LPARAM)hIcon);

        //设置文字
        wchar_t text_IDC_VERSION[64];
        swprintf_s(text_IDC_VERSION, 64, L"StartOnlyOne 版本 %ls", VERSION);
        wchar_t text_IDC_COPYRIGHT[64];
        swprintf_s(text_IDC_COPYRIGHT, 64, L"Copyright (C) %ls", COPYRIGHT);

        SetDlgItemTextW(hDlg, IDC_VERSION, text_IDC_VERSION);
        SetDlgItemTextW(hDlg, IDC_COPYRIGHT, text_IDC_COPYRIGHT);
        return TRUE;
    }
    case WM_COMMAND:
    {
        switch (LOWORD(wParam)) {
        case IDOK:
        {
            EndDialog(hDlg, IDOK);
            return TRUE;
        }
        return FALSE;
        }
    }
    return FALSE;
    }
    return FALSE;
}

// 工具主界面过程
INT_PTR CALLBACK ToolMainProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_INITDIALOG: {
        // 居中窗口
        RECT rcDlg, rcScreen;
        GetWindowRect(hDlg, &rcDlg);
        SystemParametersInfoW(SPI_GETWORKAREA, 0, &rcScreen, 0);
        int x = rcScreen.left + (rcScreen.right - rcScreen.left - (rcDlg.right - rcDlg.left)) / 2;
        int y = rcScreen.top + (rcScreen.bottom - rcScreen.top - (rcDlg.bottom - rcDlg.top)) / 2;
        SetWindowPos(hDlg, NULL, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);

        // 设置窗口图标
        HINSTANCE hInst = (HINSTANCE)GetWindowLongPtrW(hDlg, GWLP_HINSTANCE);
        HICON hIcon = LoadIconW(hInst, MAKEINTRESOURCEW(IDI_MAIN_ICON));
        SendMessageW(hDlg, WM_SETICON, ICON_SMALL, (LPARAM)hIcon);

        //设置按钮文字(默认冷却时间)
        wchar_t wCOOLDOWN[64];
        swprintf_s(wCOOLDOWN, 64, L"%d", COOLDOWN);
        wchar_t text_IDC_CONVERT_DESKTOP_ALL[64];
        swprintf_s(text_IDC_CONVERT_DESKTOP_ALL, 64, L"转换桌面快捷方式 (默认设置：冷却%ls秒)", wCOOLDOWN);
        wchar_t text_IDC_BACKUP_AND_CONVERT[64];
        swprintf_s(text_IDC_BACKUP_AND_CONVERT, 64, L"一键备份并转换 (默认设置：冷却%ls秒)", wCOOLDOWN);
        SetDlgItemTextW(hDlg, IDC_CONVERT_DESKTOP_ALL, text_IDC_CONVERT_DESKTOP_ALL);
        SetDlgItemTextW(hDlg, IDC_BACKUP_AND_CONVERT, text_IDC_BACKUP_AND_CONVERT);

        wchar_t text_IDC_VERSION_COPYRIGHT[64];
        swprintf_s(text_IDC_VERSION_COPYRIGHT, 64, L"StartOnlyOne Version:%ls    Copyright (C) %ls", VERSION, COPYRIGHT);
        SetDlgItemTextW(hDlg, IDC_VERSION_COPYRIGHT, text_IDC_VERSION_COPYRIGHT);

        return TRUE;
    }
    case WM_COMMAND: {
        switch (LOWORD(wParam)) {
        case IDC_BACKUP_DESKTOP_ALL:      // 备份按钮
            BackupDesktopAllLnk(hDlg);
            return TRUE;
        case IDC_RESTORE_BACKUP: {        // 还原按钮
            auto list = GetBackupTimestamps(LnkBackupDir);
            if (list.empty()) { MessageBoxW(hDlg, L"无备份！", L"提示", MB_OK); return TRUE; }
            if (DialogBoxParamW(GetModuleHandleW(NULL), MAKEINTRESOURCEW(IDD_SELECT_BACKUP), hDlg, SelectBackupProc, (LPARAM)&list) == IDOK) {
                RestoreBackupToDesktops(LnkBackupDir + L"\\" + g_selectedBackup, hDlg);
            }
            return TRUE;
        }
        case IDC_CONVERT_DESKTOP_ALL:    // 直接转换（使用默认冷却时间）
        {
            auto list = GetBackupTimestamps(LnkBackupDir);
            if (list.empty()) { MessageBoxW(hDlg, L"请先备份！", L"提示", MB_OK); return TRUE; }
            ConvertDesktopAllLnk(hDlg, COOLDOWN);
            return TRUE;
        }
        case IDC_BACKUP_AND_CONVERT:     // 一键备份并转换（默认冷却时间）
        {
            BackupDesktopAllLnk(hDlg);
            ConvertDesktopAllLnk(hDlg, COOLDOWN);
            MessageBoxW(hDlg, L"备份+转换完成！", L"提示", MB_OK);
            return TRUE;
        }
        case IDC_CONVERT_GUIDE:          // 转换向导（可自定义冷却）
        {
            auto list = GetBackupTimestamps(LnkBackupDir);
            if (list.empty()) { MessageBoxW(hDlg, L"请先备份！", L"提示", MB_OK); return TRUE; }
            DialogBoxParamW(GetModuleHandleW(NULL), MAKEINTRESOURCEW(IDD_CONVERT_GUIDE), hDlg, ConvertGuideProc, 0);
            return TRUE;
        }
        case IDC_BACKUP_CONVERT_GUIDE:   // 备份并转换向导
        {
            DialogBoxParamW(GetModuleHandleW(NULL), MAKEINTRESOURCEW(IDD_CONVERT_GUIDE), hDlg, BackupConvertGuideProc, 0);
            return TRUE;
        }
        case IDC_ABOUT:                 //关于
        {
            DialogBoxParamW(GetModuleHandleW(NULL), MAKEINTRESOURCEW(IDD_ABOUT), hDlg, AboutProc, 0);
            return TRUE;
        }
        case IDCANCEL:
            EndDialog(hDlg, IDCANCEL);
            return TRUE;
        }
        return FALSE;
    }
    case WM_CLOSE:
        EndDialog(hDlg, IDCANCEL);
        return TRUE;
    }
    return FALSE;
}
