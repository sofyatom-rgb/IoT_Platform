import { BrowserRouter } from 'react-router-dom'; import { Toaster } from 'sonner'; import { Router } from './router';
export function App(){return <BrowserRouter><Router/><Toaster theme="dark" richColors/></BrowserRouter>}
