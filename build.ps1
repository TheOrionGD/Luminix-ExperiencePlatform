Write-Output "Building Luminix..."

# Core files that should compile
$files = @(
    "src/main.c",
    "src/database.c",
    "src/json_io.c", 
    "src/utils.c",
    "src/index.c",
    "src/backup.c",
    "src/cli.c",
    "src/export_import.c",
    "src/parser.c",
    "src/query_engine.c",
    "src/security.c",
    "src/storage.c",
    "src/transaction.c"
)

# Compile with minimal warnings
D:\MinGW-w64\mingw32\bin\gcc.exe -I./src $files -o luminix.exe -lm

if ($LASTEXITCODE -eq 0) {
    Write-Output "Build successful!"
} else {
    Write-Output "Build failed!"
    Write-Output "Trying with reduced warnings..."
    
    # Try with fewer warnings
    D:\MinGW-w64\mingw32\bin\gcc.exe -I./src -w $files -o luminix.exe -lm
    if ($LASTEXITCODE -eq 0) {
        Write-Output "Build successful with warnings suppressed!"
    } else {
        Write-Output "Build still failed!"
    }
}
