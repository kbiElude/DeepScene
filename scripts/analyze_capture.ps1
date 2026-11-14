<#
    Analyses a capture and bakes a recipe from it, into a folder of its own: the passes (APIDumpAnalysisExtractor), the dependencies between the
    frames (APIDumpDependencyExtractor), the baked recipe (RecipeBake -o, never in place) and what RecipeBake --check says of it.

    Nothing that exists is changed or deleted: the recipe that is given is only read, and the output folder must not be the folder of the
    recipe. To put the baked recipe in use, copy it over by hand.

    Example:
      scripts\analyze_capture.ps1 -Workload captures\kasparov\result_dump.workload -Recipe apps\recipes\kasparov.json -OutDir C:\temp\kasparov_an
#>
param
(
    [Parameter(Mandatory = $true)] [string] $Workload,
    [Parameter(Mandatory = $true)] [string] $Recipe,
    [Parameter(Mandatory = $true)] [string] $OutDir,
    [string] $ToolsDir  = (Join-Path $PSScriptRoot '..\build_final\tools\analysis'),
    [int]    $MaxFrames = 0
)

$ErrorActionPreference = 'Stop'

function Invoke-Tool
{
    param ([string] $Name, [string[]] $Arguments, [string] $LogPath)

    $tool_path = Join-Path $ToolsDir ($Name + '_x64.exe')

    if (-not (Test-Path $tool_path))
    {
        throw "Missing tool: $tool_path (build the project, or pass -ToolsDir)"
    }

    $watch = [System.Diagnostics.Stopwatch]::StartNew()

    & $tool_path @Arguments *> $LogPath

    if ($LASTEXITCODE -ne 0)
    {
        Get-Content $LogPath -Tail 5
        throw "$Name failed with exit code $LASTEXITCODE (log: $LogPath)"
    }

    Write-Host ("{0,-28} done in {1:N0} s" -f $Name, $watch.Elapsed.TotalSeconds)
}

$workload_path = (Resolve-Path $Workload).Path
$recipe_path   = (Resolve-Path $Recipe).Path

if (-not (Test-Path $OutDir))
{
    New-Item -ItemType Directory -Path $OutDir | Out-Null
}

$out_path = (Resolve-Path $OutDir).Path

if ($out_path -eq (Split-Path $recipe_path -Parent))
{
    throw 'The output folder is the folder of the recipe: pick another, so that no recipe is overwritten.'
}

$name        = [System.IO.Path]::GetFileNameWithoutExtension($workload_path)
$passes_path = Join-Path $out_path ($name + '.passes')
$deps_path   = Join-Path $out_path ($name + '.deps')
$baked_path  = Join-Path $out_path ($name + '.baked.json')
$frames      = if ($MaxFrames -gt 0) { @('--max-frames', $MaxFrames) } else { @() }

Invoke-Tool 'APIDumpAnalysisExtractor'   (@($workload_path, '-o', $passes_path, '--stats') + $frames) (Join-Path $out_path 'extract_passes.log')
Invoke-Tool 'APIDumpDependencyExtractor' (@($workload_path, '-o', $deps_path,   '--stats') + $frames) (Join-Path $out_path 'extract_deps.log')
Invoke-Tool 'RecipeBake'                 @($recipe_path, $passes_path, '--deps', $deps_path, '-o', $baked_path)                     (Join-Path $out_path 'bake.log')
Invoke-Tool 'RecipeBake'                 @('--check', $baked_path, $passes_path)                                                   (Join-Path $out_path 'check.log')

Write-Host ''
Write-Host "Baked recipe: $baked_path"
Write-Host "What the check says (first lines of $(Join-Path $out_path 'check.log')):"
Get-Content (Join-Path $out_path 'check.log') -TotalCount 8
