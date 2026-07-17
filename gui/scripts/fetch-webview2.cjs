/**
 * Download Microsoft WebView2 SDK headers/libs required to build tinytron on Windows.
 *
 * Uses dotnet + nuget.org (bypasses broken custom NuGet feeds) and falls back to the
 * global NuGet package cache when already installed.
 */

const fs = require("node:fs");
const path = require("node:path");
const { spawnSync } = require("node:child_process");

const WEBVIEW2_VERSION = "1.0.2903.40";
const guiRoot = path.join(__dirname, "..");
const depsDir = path.join(guiRoot, "third_party", "tiny", "deps");
const headerPath = path.join(
  depsDir,
  "webview2",
  "build",
  "native",
  "include",
  "WebView2.h",
);

function nugetCacheHeader() {
  const home = process.env.USERPROFILE || process.env.HOME || "";
  if (!home) {
    return null;
  }

  const cached = path.join(
    home,
    ".nuget",
    "packages",
    "microsoft.web.webview2",
    WEBVIEW2_VERSION,
    "build",
    "native",
    "include",
    "WebView2.h",
  );
  return fs.existsSync(cached) ? cached : null;
}

function copyWebView2Build(sourceBuildDir, destinationRoot) {
  const destinationBuildDir = path.join(destinationRoot, "build");
  if (fs.existsSync(destinationRoot)) {
    fs.rmSync(destinationRoot, { recursive: true, force: true });
  }
  fs.mkdirSync(destinationRoot, { recursive: true });
  fs.cpSync(sourceBuildDir, destinationBuildDir, { recursive: true });
}

function fetchWithDotnet() {
  const workDir = path.join(depsDir, "webview2_fetch");
  const projectDir = path.join(workDir, "WebView2Fetch");
  const projectFile = path.join(projectDir, "WebView2Fetch.csproj");

  fs.mkdirSync(projectDir, { recursive: true });

  if (!fs.existsSync(projectFile)) {
    const create = spawnSync(
      "dotnet",
      ["new", "classlib", "-n", "WebView2Fetch", "-f", "netstandard2.0", "--force"],
      { cwd: workDir, stdio: "inherit" },
    );
    if (create.status !== 0) {
      throw new Error("dotnet new failed while preparing WebView2 fetch project");
    }
  }

  const add = spawnSync(
    "dotnet",
    [
      "add",
      "package",
      "Microsoft.Web.WebView2",
      "--version",
      WEBVIEW2_VERSION,
      "--source",
      "https://api.nuget.org/v3/index.json",
    ],
    { cwd: projectDir, stdio: "inherit" },
  );
  if (add.status !== 0) {
    throw new Error("dotnet add package failed for Microsoft.Web.WebView2");
  }

  const cachedHeader = nugetCacheHeader();
  if (!cachedHeader) {
    throw new Error(
      "Microsoft.Web.WebView2 was restored but WebView2.h was not found in the NuGet cache",
    );
  }

  const buildDir = path.join(
    path.dirname(path.dirname(path.dirname(cachedHeader))),
    "build",
  );
  copyWebView2Build(buildDir, path.join(depsDir, "webview2"));
}

function main() {
  if (process.platform !== "win32") {
    return;
  }

  if (fs.existsSync(headerPath)) {
    console.log(`WebView2 SDK already present: ${headerPath}`);
    return;
  }

  const cachedHeader = nugetCacheHeader();
  if (cachedHeader) {
    const buildDir = path.join(
      path.dirname(path.dirname(path.dirname(cachedHeader))),
      "build",
    );
    copyWebView2Build(buildDir, path.join(depsDir, "webview2"));
    console.log(`WebView2 SDK copied from NuGet cache: ${headerPath}`);
    return;
  }

  console.log(`Fetching WebView2 SDK ${WEBVIEW2_VERSION} via dotnet...`);
  fetchWithDotnet();

  if (!fs.existsSync(headerPath)) {
    throw new Error(`WebView2.h not found after fetch: ${headerPath}`);
  }

  console.log(`WebView2 SDK installed: ${headerPath}`);
}

try {
  main();
} catch (error) {
  console.error(error instanceof Error ? error.message : String(error));
  process.exit(1);
}
