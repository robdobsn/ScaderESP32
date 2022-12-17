import React, { useEffect } from 'react';
import './ScaderApp.css';
import { ScaderCommon } from './ScaderCommon';
import { ScaderManager } from './ScaderManager';
import ScaderRelays from './ScaderRelays';
import { ScaderScreenProps } from './ScaderCommon';
import ScaderShades from './ScaderShades';
import ScaderDoors from './ScaderDoors';

// const testServerPath = "http://localhost:3123";
const testServerPath = "http://192.168.86.90";

const scaderManager = ScaderManager.getInstance();
scaderManager.setTestServerPath(testServerPath);

function ScaderApp() {

  const [isEditingMode, setEditingMode] = React.useState(false);

  useEffect(() => {
    const initScaderManager = async () => {
      await scaderManager.init();
    };

    initScaderManager().catch((err) => {
      console.error(`Error initializing ScaderManager: ${err}`);
    });
  }, []);
  
  const handleMenuClick = (event: React.MouseEvent<HTMLButtonElement>) => {
    console.log("ScaderApp.handleMenuClick");
    if (isEditingMode) {
      scaderManager.revertConfig();
    }
    setEditingMode(!isEditingMode);
  };

  const handleSettingsSave = (event: React.MouseEvent<HTMLButtonElement>) => {
    console.log(`ScaderApp.handleSettingsSave isChanged {scaderManager.isConfigChanged()} newConfig ${JSON.stringify(scaderManager.getMutableConfig())}`);
    setEditingMode(false);
    scaderManager.persistConfig();
  };

  const handleSettingsCancel = (event: React.MouseEvent<HTMLButtonElement>) => {
    console.log("ScaderApp.handleSettingsCancel");
    setEditingMode(false);
    scaderManager.revertConfig();
  }

  const screenProps = new ScaderScreenProps(isEditingMode, scaderManager.getConfig());

  return (
    <div className="ScaderApp">
    <div className="ScaderApp-body">
      {/* Header */}
      <ScaderCommon config={screenProps.config} 
            onClickHamburger={handleMenuClick}
            onClickSave={handleSettingsSave}
            onClickCancel={handleSettingsCancel}
            isEditMode={isEditingMode}/>
      {/* Render screens pass config as props */}
      {<ScaderRelays {...screenProps} />}  
      {<ScaderShades {...screenProps} />}
      {<ScaderDoors {...screenProps} />}
    </div>
  </div>
  );
}
  
export default ScaderApp;
