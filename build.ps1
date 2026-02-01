Write-Output "Building Luminix..."

# Core files that should compile
$files = @(
    "src/main.c",
    "src/database.c",
    "src/json_io.c", 
    "src/utils.c",
    "src/index.c"
)

# Compile with minimal warnings
gcc -I./src $files -o luminix.exe -lm

if ($LASTEXITCODE -eq 0) {
    Write-Output "Build successful!"
    .\luminix.exe
} else {
    Write-Output "Build failed!"
    Write-Output "Trying with reduced warnings..."
    
    # Try with fewer warnings
    gcc -I./src -w $files -o luminix.exe -lm
    if ($LASTEXITCODE -eq 0) {
        Write-Output "Build successful with warnings suppressed!"
        .\luminix.exe
    } else {
        Write-Output "Build still failed!"
    }
}
