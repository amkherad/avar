import { defineConfig, loadEnv } from "vite";
import react from "@vitejs/plugin-react";
import path from "node:path";

export default defineConfig(({ mode }) => {
  const env = loadEnv(mode, process.cwd(), "");
  const buildDate = env.VITE_BUILD_DATE || new Date().toISOString();

  return {
    plugins: [react()],
    resolve: {
      alias: {
        "@": path.resolve(__dirname, "src"),
      },
    },
    base: "./",
    define: {
      "import.meta.env.VITE_BUILD_VERSION": JSON.stringify(
        env.VITE_BUILD_VERSION || process.env.npm_package_version || "dev",
      ),
      "import.meta.env.VITE_BUILD_DATE": JSON.stringify(buildDate),
      "import.meta.env.VITE_BUILD_COMMIT": JSON.stringify(env.VITE_BUILD_COMMIT || ""),
    },
    server: {
      proxy: {
        "/api": {
          target: "http://127.0.0.1:8000",
          changeOrigin: true,
          ws: true,
        },
      },
    },
    preview: {
      proxy: {
        "/api": {
          target: "http://127.0.0.1:8000",
          changeOrigin: true,
          ws: true,
        },
      },
    },
  };
});
