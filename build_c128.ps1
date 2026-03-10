# --- Configuration ---
$vicePath  = "C:\Program Files\GTK3VICE-3.10-win64\bin\c1541.exe"
$viceC128  = "C:\Program Files\GTK3VICE-3.10-win64\bin\x128.exe"
$viceC64  = "C:\Program Files\GTK3VICE-3.10-win64\bin\x64.exe"
$vicevic20  = "C:\Program Files\GTK3VICE-3.10-win64\bin\xvic.exe"
$viceCPet  = "C:\Program Files\GTK3VICE-3.10-win64\bin\xpet.exe"
$viceCPlus4  = "C:\Program Files\GTK3VICE-3.10-win64\bin\xplus4.exe"
$cl65Path  = "C:\cc65-snapshot-win32\bin\cl65.exe"
$buildDir  = "X:\my source code\text adventiure game cc65\build"
$diskImage = "adventure.d64"

# --- 1. Compile world data ---
Write-Host "Compiling world data..." -ForegroundColor Cyan
python3.8.exe .\make_ui.py
if ($LASTEXITCODE -ne 0) { Write-Host "ERROR: make_ui.py failed." -ForegroundColor Red; exit 1 }
Write-Host "Compiling world data..." -ForegroundColor Cyan
python3.8.exe .\world_compiler.py
if ($LASTEXITCODE -ne 0) { Write-Host "ERROR: world_compiler.py failed." -ForegroundColor Red; exit 1 }

# --- 2. Build game.prg ---
Write-Host "Building game.prg..." -ForegroundColor Cyan
& "$cl65Path" -t c128 .\adventure_engine.c  -Ln game.lbl -o game.prg
if ($LASTEXITCODE -ne 0) { Write-Host "ERROR: game.prg build failed." -ForegroundColor Red; exit 1 }

# --- 3. Stage files ---
if (!(Test-Path $buildDir)) { New-Item -ItemType Directory $buildDir }
Copy-Item "game.prg"  -Destination "$buildDir\game.prg"
Copy-Item "boot.dat"  -Destination "$buildDir\boot.dat"
Copy-Item "rooms.dat" -Destination "$buildDir\rooms.dat"


# --- 4. Build disk image ---
Push-Location $buildDir
if (Test-Path $diskImage) { Remove-Item $diskImage }

Write-Host "Creating disk: $diskImage" -ForegroundColor Cyan
& "$vicePath" -format "adventure,01" d64 $diskImage

Write-Host "Writing game.prg..." -ForegroundColor DarkGray
& "$vicePath" -attach $diskImage -write "game.prg" "game.prg"

Write-Host "Writing rooms.dat..." -ForegroundColor DarkGray
& "$vicePath" -attach $diskImage -write "rooms.dat" "rooms.dat,s"

Write-Host "Writing boot.dat..." -ForegroundColor DarkGray
& "$vicePath" -attach $diskImage -write "boot.dat" "boot.dat,s"



Write-Host ""
Write-Host "Disk contents:" -ForegroundColor Green
Write-Host "  game.prg  -- the game" -ForegroundColor Green
Write-Host "  rooms.dat -- room data (244 bytes/room, sequential)" -ForegroundColor Green
Write-Host "  boot.dat  -- RAM data (items, actors)" -ForegroundColor Green
Write-Host ""

# --- 5. Launch ---
Write-Host "Press ENTER to launch VICE, or Q to quit." -ForegroundColor Yellow
$key = $Host.UI.RawUI.ReadKey("NoEcho,IncludeKeyDown")
if ($key.Character -eq 'q' -or $key.Character -eq 'Q') {
    Pop-Location; exit 0
}

Write-Host "Launching VICE C128..." -ForegroundColor Yellow
Start-Process -FilePath "$viceC128" -ArgumentList "-8 `"$diskImage`""

Pop-Location
Write-Host "Build complete!" -ForegroundColor Green