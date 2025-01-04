# -*- mode: python ; coding: utf-8 -*-

a = Analysis(
    ['src\\PC_BLE_APP.py'],
    pathex=['src'],
    binaries=[
        ('C:\\Windows\\System32\\BluetoothApis.dll', '.'),
    ],
    datas=[],
    hiddenimports=[
        'bleak',
        'bleak.backends.winrt',
        'bleak_winrt',
        'bleak_winrt.windows',
        'async_timeout',
        'winrt',
        'winrt.windows',
        'winrt.windows.foundation',
        'winrt.windows.foundation.collections',
    ],
    hookspath=[],
    hooksconfig={},
    runtime_hooks=[],
    excludes=[],
    noarchive=False,
    optimize=0,
)
pyz = PYZ(a.pure)

exe = EXE(
    pyz,
    a.scripts,
    a.binaries,
    a.datas,
    [],
    name='Nano33 Head Tracker - PC BLE',
    debug=False,
    bootloader_ignore_signals=False,
    strip=False,
    upx=True,
    upx_exclude=[],
    runtime_tmpdir="temp",
    console=False,
    disable_windowed_traceback=False,
    argv_emulation=False,
    target_arch=None,
    codesign_identity=None,
    entitlements_file=None,
)
