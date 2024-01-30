import React, { useEffect } from 'react';
import { ScaderScreenProps } from './ScaderCommon';
import { ScaderManager } from './ScaderManager';
import { ScaderPulseCounterStates, ScaderState } from './ScaderState';

const scaderManager = ScaderManager.getInstance();

export default function ScaderPulseCounter(props:ScaderScreenProps) {

  const scaderName = "ScaderPulseCounter";
  const pulseCountElemName = "pulseCount";
  const restCommandName = "pulsecount/value";
  const [config, setConfig] = React.useState(props.config[scaderName]);
  const [state, setState] = React.useState(new ScaderState()[scaderName]);

  useEffect(() => {
    scaderManager.onConfigChange((newConfig) => {
      console.log(`${scaderName}.onConfigChange`);
      // Update config
      setConfig(newConfig[scaderName]);
    });
    scaderManager.onStateChange(scaderName, (newState) => {
      console.log(`${scaderName} onStateChange ${JSON.stringify(newState)}`);
      // Update state
      if (pulseCountElemName in newState) {
        setState(new ScaderPulseCounterStates(newState));
      }
    });
  }, []);

  const updateMutableConfig = (newConfig: any) => {
    // Update ScaderManager
    scaderManager.getMutableConfig()[scaderName] = newConfig;
  }

  const handleEnableChange = (event: React.ChangeEvent<HTMLInputElement>) => {
    console.log(`${scaderName}.handleEnableChange`);
    // Update config
    const newConfig = {...config, enable: event.target.checked};
    setConfig(newConfig);
    updateMutableConfig(newConfig);
  };

  const handlePulseCountNameChange = (event: React.ChangeEvent<HTMLInputElement>) => {
    console.log(`${scaderName}.handlePulseCountNameChange ${event.target.value}`);
    // Update config
    const newConfig = {...config, name: event.target.value};
    setConfig(newConfig);
    updateMutableConfig(newConfig);
  }

  const editModeScreen = () => {
    return (
      <div className="ScaderElem-edit">
        <div className="ScaderElem-editmode">
          {/* Checkbox for enable with label */}
          <label>
            <input className="ScaderElem-checkbox" type="checkbox" 
                  checked={config.enable} 
                  onChange={handleEnableChange} />
            Enable {scaderName}
          </label>
        </div>
        {config.enable && 
          <div className="ScaderElem-body">
            {/* Pulse count */}
            <div>
              <label>
                Current Pulse Count {state[pulseCountElemName]}
              </label>
            </div>
            <div>
              <label>
                Pulse Count:
                <input className="ScaderElem-input" type="number" id={`scader-pulseCount`} placeholder='enter count'  />
                <button className="ScaderElem-button-editmode" 
                          onClick={(event) => {
                            scaderManager.sendCommand(`/${restCommandName}/${(document.getElementById(`scader-pulseCount`) as HTMLInputElement).value}`);
                          }}
                          id={`scader-pulseCountBtn`}>
                      Set
                </button>
              </label>
            </div>
            <div>
              <label>
                Pulse count name:
                <input className="ScaderElem-input" type="text" id={`scader-pulseCountName`} value={config.name} onChange={handlePulseCountNameChange} />
              </label>
            </div>
          </div>
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
            { /* Pulse count label */ }
            <div className="ScaderElem-status">
              <label>{config.name}</label>
              {state[pulseCountElemName]}
            </div>
          </div>
        </div>
      : null
    )
  }

  return (
    // Show appropriate screen based on mode
    props.isEditingMode ? editModeScreen() : normalModeScreen()
  );
}
