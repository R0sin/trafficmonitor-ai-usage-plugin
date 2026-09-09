$ErrorActionPreference = 'Stop'
$helperPath = Join-Path $PSScriptRoot 'helper\codex-api-usage.mjs'
if (-not (Test-Path -LiteralPath $helperPath)) {
    $helperPath = Join-Path (Split-Path $PSScriptRoot -Parent) 'helper\codex-api-usage\index.mjs'
}
& node $helperPath
exit $LASTEXITCODE
