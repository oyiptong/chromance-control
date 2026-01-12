import { defineConfig } from "astro/config";
import preact from "@astrojs/preact";

export default defineConfig({
  output: "static",
  integrations: [preact()],
  build: {
    assets: "assets",
    format: "directory"
  },
  vite: {
    build: {
      sourcemap: false
    }
  }
});

