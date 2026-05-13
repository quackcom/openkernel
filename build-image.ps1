# Script to create bootable disk image for QEMU
# Usage: .\build-image.ps1

$bootFile = "build/boot_sector.bin"
$kernelFile = "build/kernel.elf"
$imageFile = "build/openkernel.img"
$imageSize = 1474560  # 1.44MB floppy

# Check if files exist
if (!(Test-Path $bootFile)) {
    Write-Error "Boot sector not found: $bootFile"
    exit 1
}

if (!(Test-Path $kernelFile)) {
    Write-Error "Kernel not found: $kernelFile"
    exit 1
}

# Create blank image
Write-Host "Creating disk image..."
$fs = [System.IO.File]::OpenWrite($imageFile)

# Write boot sector
$boot = [System.IO.File]::ReadAllBytes($bootFile)
$fs.Write($boot, 0, $boot.Length)

# Write kernel at sector 1 (512 bytes offset)
$kernel = [System.IO.File]::ReadAllBytes($kernelFile)
$fs.Seek(512, 0)
$fs.Write($kernel, 0, $kernel.Length)

$fs.Close()

Write-Host "Disk image created: $imageFile"
Write-Host "Run: qemu-system-i386 -fda $imageFile -serial stdio"
