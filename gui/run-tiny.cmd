@echo off
cd /d %~dp0
if not exist "dist\index.html" (
  echo Building GUI...
  call npm run build
)
if not exist "node_modules\tinytron\build\Release\addon.node" (
  echo Building tinytron native addon...
  node scripts\install-tinytron.cjs
)
node tiny\main.cjs
