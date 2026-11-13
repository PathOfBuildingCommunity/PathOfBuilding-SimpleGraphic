param(
	[Parameter(Mandatory)][string]$Runtime,
	[Parameter(Mandatory)][string]$Fonts,
	[Parameter(Mandatory)][string]$Output
)
$ErrorActionPreference = 'Stop'
$Runtime = (Resolve-Path $Runtime).Path
$Fonts = (Resolve-Path $Fonts).Path
if (Test-Path $Output) { throw 'Use a new output directory for the isolated runtime tests' }
$Output = (New-Item -ItemType Directory -Path $Output).FullName
Copy-Item "$Runtime/*.dll" $Output
Copy-Item "$Runtime/SimpleGraphic" $Output -Recurse
Copy-Item "$Fonts/*" "$Output/SimpleGraphic/Fonts" -Force
Copy-Item "$PSScriptRoot/cyrillic-render.lua" "$Output/test.lua"
Copy-Item "$PSScriptRoot/cyrillic-input.lua" "$Output/input.lua"
& cl /nologo /std:c++20 /EHsc "/Fe:$Output/runner.exe" "/Fo:$Output/runner.obj" "$PSScriptRoot/windows-runner.cpp" /link user32.lib
if ($LASTEXITCODE -ne 0) { throw 'Could not compile the Windows test driver' }
foreach ($test in @(@('test.lua', 'render-result.txt'), @('input.lua', 'input-result.txt'))) {
	$process = Start-Process "$Output/runner.exe" -ArgumentList $test[0] -WorkingDirectory $Output -WindowStyle Hidden -PassThru
	if (-not $process.WaitForExit(20000)) {
		Stop-Process -Id $process.Id
		throw "Runtime test timed out: $($test[0])"
	}
	$result = Get-Content "$Output/$($test[1])"
	Write-Output $result[-1]
	if ($process.ExitCode -ne 0 -or $result -match '^FAIL:' -or $result[-1] -notmatch '^[1-9][0-9]* tests, 0 failures$') {
		throw "Runtime test failed: $($test[0]); see $Output/$($test[1])"
	}
}
