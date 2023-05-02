// Copyright (c) 2023 Advanced Micro Devices, Inc. All rights reserved.
//
// This file is part of the AMD Render Pipeline Shaders SDK which is
// released under the AMD INTERNAL EVALUATION LICENSE.
//
// See file LICENSE.txt for full license details.

#pragma once

#include <ShlObj.h>
#include <string>
#include <unordered_map>
#include <functional>

class FileMonitor
{
public:
    static constexpr DWORD UM_FILE_CHANGED = WM_USER + 4097;

    void SetNotificationCallback(std::function<void(const std::string&)> func)
    {
        m_callback = func;
    }

    bool BeginWatch(HWND hWndListener, const std::string& fileName)
    {
        auto iter = m_registerIDs.find(fileName);
        if (iter != m_registerIDs.end())
        {
            return false;
        }

        SHChangeNotifyEntry notifyEntry = {};
        notifyEntry.pidl                = ILCreateFromPathA(fileName.c_str());

        ULONG uId = SHChangeNotifyRegister(hWndListener,
                                           SHCNRF_InterruptLevel | SHCNRF_NewDelivery,
                                           SHCNE_RENAMEITEM | SHCNE_DELETE | SHCNE_UPDATEITEM,
                                           UM_FILE_CHANGED,
                                           1,
                                           &notifyEntry);

        return (uId != 0);
    }

    void EndWatch(const std::string& fileName)
    {
        auto iter = m_registerIDs.find(fileName);
        if (iter != m_registerIDs.end())
        {
            SHChangeNotifyDeregister(iter->second);
            m_registerIDs.erase(iter);
        }
    }

    bool HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam) const
    {
        if (msg == UM_FILE_CHANGED)
        {
            PIDLIST_ABSOLUTE* ppidl;
            LONG              eventID;

            HANDLE hLock = SHChangeNotification_Lock(HANDLE(wParam), DWORD(lParam), &ppidl, &eventID);

            if (hLock)
            {
                std::string srcPath, dstPath;

                CHAR buf[MAX_PATH];

                if (ppidl[0])
                {
                    SHGetPathFromIDListA(ppidl[0], buf);
                    srcPath = buf;
                }

                if (ppidl[1])
                {
                    SHGetPathFromIDListA(ppidl[1], buf);
                    srcPath = buf;
                }

                SHChangeNotification_Unlock(hLock);

                if (eventID == SHCNE_UPDATEITEM)
                {
                    printf("\nFile changed: %s", srcPath.c_str());

                    if (m_callback)
                    {
                        m_callback(srcPath);
                    }
                }
            }

            return true;
        }

        return false;
    }

private:
    std::unordered_map<std::string, ULONG> m_registerIDs;
    std::function<void(const std::string&)> m_callback;
};
