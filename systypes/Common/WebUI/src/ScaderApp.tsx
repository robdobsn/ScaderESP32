import React, { useEffect } from 'react';
import { ScaderCommon } from './ScaderCommon';
import { ScaderManager } from './ScaderManager';
import ScaderRelays from './ScaderRelays';
import ScaderElecMeters from './ScaderElecMeters';
import { ScaderScreenProps } from './ScaderCommon';
import ScaderShades from './ScaderShades';
import ScaderLocks from './ScaderLocks';
import ScaderLEDPix from './ScaderLEDPix';
import ScaderOpener from './ScaderOpener';
import ScaderRFID from './ScaderRFID';
import ScaderPulseCounter from './ScaderPulseCounter';
import ReactDOM from 'react-dom';
import './ScaderApp.css';

// const testServerPath = "http://localhost:3123";
const testServerPath = "http://192.168.86.105";

ScaderManager.getInstance().setTestServerPath(testServerPath);

export default function ScaderApp() {

  const [isEditingMode, setEditingMode] = React.useState(false);

  useEffect(() => {
    const initScaderManager = async () => {
      console.log(`ScaderApp.initScaderManager`);
      await ScaderManager.getInstance().init();
    };

    initScaderManager().catch((err) => {
      console.error(`Error initializing ScaderManager: ${err}`);
    });
  }, []);
  
  const handleMenuClick = (event: React.MouseEvent<HTMLButtonElement>) => {
    console.log("ScaderApp.handleMenuClick");
    if (isEditingMode) {
      ScaderManager.getInstance().revertConfig();
    }
    setEditingMode(!isEditingMode);
  };

  const handleSettingsSave = (event: React.MouseEvent<HTMLButtonElement>) => {
    console.log(`ScaderApp.handleSettingsSave isChanged ${ScaderManager.getInstance().isConfigChanged()} newConfig ${JSON.stringify(ScaderManager.getInstance().getMutableConfig())}`);
    setEditingMode(false);
    ScaderManager.getInstance().persistConfig();
  };

  const handleSettingsCancel = (event: React.MouseEvent<HTMLButtonElement>) => {
    console.log("ScaderApp.handleSettingsCancel");
    setEditingMode(false);
    ScaderManager.getInstance().revertConfig();
  }

  const screenProps = new ScaderScreenProps(isEditingMode, ScaderManager.getInstance().getConfig());

  const bodyClasses = isEditingMode ? "ScaderApp-bodyeditmode" : "ScaderApp-body";
  return (
    <div className="ScaderApp h-full">
    <div className={bodyClasses}>
      {/* Header */}
      <ScaderCommon config={screenProps.config} 
            onClickHamburger={handleMenuClick}
            onClickSave={handleSettingsSave}
            onClickCancel={handleSettingsCancel}
            isEditMode={isEditingMode}/>
      {/* Render screens pass config as props */}
      {<ScaderRelays {...screenProps} />}
      {<ScaderShades {...screenProps} />}
      {<ScaderLocks {...screenProps} />}
      {<ScaderLEDPix {...screenProps} />}
      {<ScaderOpener {...screenProps} />}
      {<ScaderRFID {...screenProps} />}
      {<ScaderPulseCounter {...screenProps} />}
      {<ScaderElecMeters {...screenProps} />}
    </div>
  </div>
  );
}

ReactDOM.render(<ScaderApp />, document.getElementById('root'));