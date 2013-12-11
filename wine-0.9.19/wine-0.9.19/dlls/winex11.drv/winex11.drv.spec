# GDI driver

@ cdecl AlphaBlend(ptr long long long long ptr long long long long long) X11DRV_AlphaBlend
@ cdecl Arc(ptr long long long long long long long long) X11DRV_Arc
@ cdecl BitBlt(ptr long long long long ptr long long long) X11DRV_BitBlt
@ cdecl ChoosePixelFormat(ptr ptr) X11DRV_ChoosePixelFormat
@ cdecl Chord(ptr long long long long long long long long) X11DRV_Chord
@ cdecl CreateBitmap(ptr long ptr) X11DRV_CreateBitmap
@ cdecl CreateDC(long ptr wstr wstr wstr ptr) X11DRV_CreateDC
@ cdecl CreateDIBSection(ptr long ptr long) X11DRV_CreateDIBSection
@ cdecl DeleteBitmap(long) X11DRV_DeleteBitmap
@ cdecl DeleteDC(ptr) X11DRV_DeleteDC
@ cdecl DescribePixelFormat(ptr long long ptr) X11DRV_DescribePixelFormat
@ cdecl Ellipse(ptr long long long long) X11DRV_Ellipse
@ cdecl EnumDeviceFonts(ptr ptr ptr long) X11DRV_EnumDeviceFonts
@ cdecl ExtEscape(ptr long long ptr long ptr) X11DRV_ExtEscape
@ cdecl ExtFloodFill(ptr long long long long) X11DRV_ExtFloodFill
@ cdecl ExtTextOut(ptr long long long ptr ptr long ptr) X11DRV_ExtTextOut
@ cdecl GetBitmapBits(long ptr long) X11DRV_GetBitmapBits
@ cdecl GetCharWidth(ptr long long ptr) X11DRV_GetCharWidth
@ cdecl GetDCOrgEx(ptr ptr) X11DRV_GetDCOrgEx
@ cdecl GetDIBColorTable(ptr long long ptr) X11DRV_GetDIBColorTable
@ cdecl GetDIBits(ptr long long long ptr ptr long) X11DRV_GetDIBits
@ cdecl GetDeviceCaps(ptr long) X11DRV_GetDeviceCaps
@ cdecl GetDeviceGammaRamp(ptr ptr) X11DRV_GetDeviceGammaRamp
@ cdecl GetNearestColor(ptr long) X11DRV_GetNearestColor
@ cdecl GetPixel(ptr long long) X11DRV_GetPixel
@ cdecl GetPixelFormat(ptr) X11DRV_GetPixelFormat
@ cdecl GetSystemPaletteEntries(ptr long long ptr) X11DRV_GetSystemPaletteEntries
@ cdecl GetTextExtentExPoint(ptr ptr long long ptr ptr ptr) X11DRV_GetTextExtentExPoint
@ cdecl GetTextMetrics(ptr ptr) X11DRV_GetTextMetrics
@ cdecl LineTo(ptr long long) X11DRV_LineTo
@ cdecl PaintRgn(ptr long) X11DRV_PaintRgn
@ cdecl PatBlt(ptr long long long long long) X11DRV_PatBlt
@ cdecl Pie(ptr long long long long long long long long) X11DRV_Pie
@ cdecl PolyPolygon(ptr ptr ptr long) X11DRV_PolyPolygon
@ cdecl PolyPolyline(ptr ptr ptr long) X11DRV_PolyPolyline
@ cdecl Polygon(ptr ptr long) X11DRV_Polygon
@ cdecl Polyline(ptr ptr long) X11DRV_Polyline
@ cdecl RealizeDefaultPalette(ptr) X11DRV_RealizeDefaultPalette
@ cdecl RealizePalette(ptr long long) X11DRV_RealizePalette
@ cdecl Rectangle(ptr long long long long) X11DRV_Rectangle
@ cdecl RoundRect(ptr long long long long long long) X11DRV_RoundRect
@ cdecl SelectBitmap(ptr long) X11DRV_SelectBitmap
@ cdecl SelectBrush(ptr long) X11DRV_SelectBrush
@ cdecl SelectFont(ptr long long) X11DRV_SelectFont
@ cdecl SelectPen(ptr long) X11DRV_SelectPen
@ cdecl SetBitmapBits(long ptr long) X11DRV_SetBitmapBits
@ cdecl SetBkColor(ptr long) X11DRV_SetBkColor
@ cdecl SetDCBrushColor(ptr long) X11DRV_SetDCBrushColor
@ cdecl SetDCOrg(ptr long long) X11DRV_SetDCOrg
@ cdecl SetDCPenColor(ptr long) X11DRV_SetDCPenColor
@ cdecl SetDIBColorTable(ptr long long ptr) X11DRV_SetDIBColorTable
@ cdecl SetDIBits(ptr long long long ptr ptr long) X11DRV_SetDIBits
@ cdecl SetDIBitsToDevice(ptr long long long long long long long long ptr ptr long) X11DRV_SetDIBitsToDevice
@ cdecl SetDeviceClipping(ptr long long) X11DRV_SetDeviceClipping
@ cdecl SetDeviceGammaRamp(ptr ptr) X11DRV_SetDeviceGammaRamp
@ cdecl SetPixel(ptr long long long) X11DRV_SetPixel
@ cdecl SetPixelFormat(ptr long ptr) X11DRV_SetPixelFormat
@ cdecl SetTextColor(ptr long) X11DRV_SetTextColor
@ cdecl StretchBlt(ptr long long long long ptr long long long long long) X11DRV_StretchBlt
@ cdecl SwapBuffers(ptr) X11DRV_SwapBuffers

# USER driver

@ cdecl ActivateKeyboardLayout(long long) X11DRV_ActivateKeyboardLayout
@ cdecl Beep() X11DRV_Beep
@ cdecl GetAsyncKeyState(long) X11DRV_GetAsyncKeyState
@ cdecl GetKeyNameText(long ptr long) X11DRV_GetKeyNameText
@ cdecl GetKeyboardLayout(long) X11DRV_GetKeyboardLayout
@ cdecl GetKeyboardLayoutList(long ptr) X11DRV_GetKeyboardLayoutList
@ cdecl GetKeyboardLayoutName(ptr) X11DRV_GetKeyboardLayoutName
@ cdecl LoadKeyboardLayout(wstr long) X11DRV_LoadKeyboardLayout
@ cdecl MapVirtualKeyEx(long long long) X11DRV_MapVirtualKeyEx
@ cdecl SendInput(long ptr long) X11DRV_SendInput
@ cdecl ToUnicodeEx(long long ptr ptr long long long) X11DRV_ToUnicodeEx
@ cdecl UnloadKeyboardLayout(long) X11DRV_UnloadKeyboardLayout
@ cdecl VkKeyScanEx(long long) X11DRV_VkKeyScanEx
@ cdecl SetCursor(ptr) X11DRV_SetCursor
@ cdecl GetCursorPos(ptr) X11DRV_GetCursorPos
@ cdecl SetCursorPos(long long) X11DRV_SetCursorPos
@ cdecl GetScreenSaveActive() X11DRV_GetScreenSaveActive
@ cdecl SetScreenSaveActive(long) X11DRV_SetScreenSaveActive
@ cdecl ChangeDisplaySettingsEx(ptr ptr long long long) X11DRV_ChangeDisplaySettingsEx
@ cdecl EnumDisplaySettingsEx(ptr long ptr long) X11DRV_EnumDisplaySettingsEx
@ cdecl AcquireClipboard(long) X11DRV_AcquireClipboard
@ cdecl CountClipboardFormats() X11DRV_CountClipboardFormats
@ cdecl CreateDesktopWindow(long) X11DRV_CreateDesktopWindow
@ cdecl CreateWindow(long ptr long) X11DRV_CreateWindow
@ cdecl DestroyWindow(long) X11DRV_DestroyWindow
@ cdecl EmptyClipboard(long) X11DRV_EmptyClipboard
@ cdecl EndClipboardUpdate() X11DRV_EndClipboardUpdate
@ cdecl EnumClipboardFormats(long) X11DRV_EnumClipboardFormats
@ cdecl GetClipboardData(long ptr ptr) X11DRV_GetClipboardData
@ cdecl GetClipboardFormatName(long str long) X11DRV_GetClipboardFormatName
@ cdecl GetDCEx(long long long) X11DRV_GetDCEx
@ cdecl IsClipboardFormatAvailable(long) X11DRV_IsClipboardFormatAvailable
@ cdecl MsgWaitForMultipleObjectsEx(long ptr long long long) X11DRV_MsgWaitForMultipleObjectsEx
@ cdecl RegisterClipboardFormat(wstr) X11DRV_RegisterClipboardFormat
@ cdecl ReleaseDC(long long long) X11DRV_ReleaseDC
@ cdecl ScrollDC(long long long ptr ptr long ptr) X11DRV_ScrollDC
@ cdecl SetClipboardData(long long long long) X11DRV_SetClipboardData
@ cdecl SetFocus(long) X11DRV_SetFocus
@ cdecl SetParent(long long) X11DRV_SetParent
@ cdecl SetWindowIcon(long long long) X11DRV_SetWindowIcon
@ cdecl SetWindowPos(ptr) X11DRV_SetWindowPos
@ cdecl SetWindowRgn(long long long) X11DRV_SetWindowRgn
@ cdecl SetWindowStyle(ptr long) X11DRV_SetWindowStyle
@ cdecl SetWindowText(long wstr) X11DRV_SetWindowText
@ cdecl ShowWindow(long long) X11DRV_ShowWindow
@ cdecl SysCommandSizeMove(long long) X11DRV_SysCommandSizeMove
@ cdecl WindowFromDC(long) X11DRV_WindowFromDC
@ cdecl WindowMessage(long long long long) X11DRV_WindowMessage

# WinTab32
@ cdecl AttachEventQueueToTablet(long) X11DRV_AttachEventQueueToTablet
@ cdecl GetCurrentPacket(ptr) X11DRV_GetCurrentPacket
@ cdecl LoadTabletInfo(long) X11DRV_LoadTabletInfo
@ cdecl WTInfoA(long long ptr) X11DRV_WTInfoA

# X11 locks
@ cdecl -norelay wine_tsx11_lock()
@ cdecl -norelay wine_tsx11_unlock()

# Desktop
@ cdecl wine_create_desktop(long long) X11DRV_create_desktop

# XIM
@ cdecl ForceXIMReset(long) X11DRV_ForceXIMReset
