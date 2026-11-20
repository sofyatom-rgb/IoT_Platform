import '@fontsource-variable/inter';
import '@fontsource-variable/space-grotesk';
import '@fontsource-variable/jetbrains-mono';
import './styles/globals.css';
import { createRoot } from 'react-dom/client';
import { App } from './app/App';
createRoot(document.getElementById('root')!).render(<App />);
