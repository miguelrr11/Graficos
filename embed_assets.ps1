# embed_assets.ps1
# Convierte los archivos de assets en arrays de bytes C++ para crear un ejecutable autocontenido.
# Ejecutar desde la raiz del proyecto antes de compilar.

$assets = @(
    "cesped.jpg",
    "madera2.jpg",
    "hoyo.png",
    "bola2.png",
    "ice4.png",
    "sand4.png",
    "beep3.wav",
    "alphabet obj.obj",
    "Balatro.mp3"
)

$out = [System.Text.StringBuilder]::new()
[void]$out.AppendLine("#pragma once")
[void]$out.AppendLine("// Auto-generado por embed_assets.ps1 - no editar manualmente")
[void]$out.AppendLine("#include <cstddef>")
[void]$out.AppendLine("")

foreach ($asset in $assets) {
    $path = Join-Path $PSScriptRoot "assets\$asset"
    if (-not (Test-Path $path)) {
        Write-Warning "No se encontro: $path"
        continue
    }
    $bytes = [System.IO.File]::ReadAllBytes($path)
    $varName = ($asset -replace '[.\- ]', '_')

    [void]$out.AppendLine("// $asset ($($bytes.Length) bytes)")
    [void]$out.AppendLine("static const unsigned char asset_$varName[] = {")

    $sb = [System.Text.StringBuilder]::new()
    for ($i = 0; $i -lt $bytes.Length; $i++) {
        if ($i % 16 -eq 0) { [void]$sb.Append("    ") }
        [void]$sb.Append(("0x{0:X2}" -f $bytes[$i]))
        if ($i -lt $bytes.Length - 1) { [void]$sb.Append(",") }
        if (($i + 1) % 16 -eq 0 -or $i -eq $bytes.Length - 1) { [void]$sb.AppendLine() }
        else { [void]$sb.Append(" ") }
    }
    [void]$out.Append($sb.ToString())
    [void]$out.AppendLine("};")
    [void]$out.AppendLine("static const unsigned int asset_${varName}_size = $($bytes.Length);")
    [void]$out.AppendLine("")

    Write-Host "  Embebido: $asset ($($bytes.Length) bytes)"
}

$headerPath = Join-Path $PSScriptRoot "include\embedded_assets.h"
[System.IO.File]::WriteAllText($headerPath, $out.ToString(), [System.Text.Encoding]::UTF8)
Write-Host "Generado: include\embedded_assets.h"
