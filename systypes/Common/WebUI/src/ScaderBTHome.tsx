import React, { useEffect } from 'react';
import { ScaderScreenProps } from './ScaderCommon';
import { ScaderManager } from './ScaderManager';
import { ScaderConfig } from './ScaderConfig';

const scaderManager = ScaderManager.getInstance();
type ScaderBTHomeConfig = ScaderConfig['ScaderBTHome'];

export default function ScaderBTHome(props: ScaderScreenProps) {

  const scaderName = "ScaderBTHome";
  const [config, setConfig] = React.useState(props.config[scaderName]);

  useEffect(() => {
    scaderManager.onConfigChange((newConfig) => {
      console.log(`${scaderName}.onConfigChange`);
      // Update config
      setConfig(newConfig[scaderName]);
    });
    scaderManager.onStateChange(scaderName, (newState) => {
      console.log(`${scaderName} onStateChange ${JSON.stringify(newState)}`);
    });
  }, []);

  const updateMutableConfig = (newConfig: ScaderBTHomeConfig) => {
    // Update ScaderManager
    scaderManager.getMutableConfig()[scaderName] = newConfig;
  }

  const handleEnableChange = (event: React.ChangeEvent<HTMLInputElement>) => {
    console.log(`${scaderName}.handleEnableChange`);
    // Update config
    const newConfig = { ...config, enable: event.target.checked };
    setConfig(newConfig);
    updateMutableConfig(newConfig);
  };


  const editModeScreen = () => {
    return (
      <div className="ScaderElem-edit">
        <div className="ScaderElem-editmode">
          {/* Checkbox for enable shades */}
          <label>
            <input className="ScaderElem-checkbox" type="checkbox" 
                  checked={config.enable} 
                  onChange={handleEnableChange} />
            Enable {scaderName}
          </label>
        </div>
        {
        }
      </div>
    );
  }
   
  const normalModeScreen = () => {
    return (
      // Display if enabled
      config.enable ?
        <div className="ScaderElem">
          <div className="ScaderElem-header">
          </div>
        </div>
        : null
    )
  }

  return (
    props.isEditingMode ? editModeScreen() : normalModeScreen()
  );
}
