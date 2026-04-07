import {defineConfig} from 'vite'
import vue from '@vitejs/plugin-vue'

export default defineConfig({
    plugins: [vue()],
    build: {
        outDir: '../public',
        emptyOutDir: false
    },
    server: {
        proxy: {
            '/admin/api': {
                target: 'http://localhost:7778',
                changeOrigin: true
            },
            '/admin/ws': {
                target: 'ws://localhost:7778',
                ws: true
            }
        }
    }
})