Get-Process | % { if ($_.MainWindowTitle -match "^(\S*\s+)?usb$" -and $_.ProcessName -match "cmd" -and $_.StartTime -gt (Get-Date).AddSeconds(-10)) { $hWnd = $_.MainWindowHandle } }
if ($hWnd) { $null = (Add-Type -namespace "pinvoke" -name "setWindowPos" -passThru -memberDefinition @"
 [DllImport("user32.dll")][return: MarshalAs(UnmanagedType.Bool)]
 public static extern bool SetWindowPos(IntPtr hWnd, IntPtr hWndInsertAfter, int x, int y, int cx, int cy, uint flags);
"@)::SetWindowPos($hWnd, -1, 8, (gwmi win32_videocontroller).CurrentVerticalResolution-160-168 - 60, 0, 0, 0x0001) } # topMost, x, y(lines x 12 + 36), w, h, noSize
