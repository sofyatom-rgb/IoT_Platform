import { defineConfig } from 'vite';
import react from '@vitejs/plugin-react';
export default defineConfig({ plugins:[react()], server:{proxy:{'/api/core':{target:'http://127.0.0.1:8080',changeOrigin:true,rewrite:p=>p.replace(/^\/api\/core/,'')},'/api/rules':{target:'http://127.0.0.1:8083',changeOrigin:true,rewrite:p=>p.replace(/^\/api\/rules/,'')},'/api/notifications':{target:'http://127.0.0.1:8082',changeOrigin:true,rewrite:p=>p.replace(/^\/api\/notifications/,'')}}}});
