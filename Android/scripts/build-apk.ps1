param(
    [string]$Sdl2Dir = $env:SDL2_DIR,
    [string]$Task = ":app:assembleDebug"
)

$androidStudioJbr = "C:\Program Files\Android\Android Studio\jbr"
if ([string]::IsNullOrWhiteSpace($env:JAVA_HOME) -and (Test-Path "$androidStudioJbr\bin\java.exe")) {
    $env:JAVA_HOME = $androidStudioJbr
}

if ([string]::IsNullOrWhiteSpace($Sdl2Dir)) {
    & "$PSScriptRoot\..\gradlew.bat" -p "$PSScriptRoot\.." $Task
} else {
    & "$PSScriptRoot\..\gradlew.bat" -p "$PSScriptRoot\.." $Task "-Psdl2Dir=$Sdl2Dir"
}
