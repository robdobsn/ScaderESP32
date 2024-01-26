import React from 'react';
import ReactDOM from 'react-dom/client';
import './index.css';
import ScaderApp from './ScaderApp';

const root = ReactDOM.createRoot(
  document.getElementById('root') as HTMLElement
);
root.render(
  <React.StrictMode>
    <ScaderApp />
  </React.StrictMode>
);
