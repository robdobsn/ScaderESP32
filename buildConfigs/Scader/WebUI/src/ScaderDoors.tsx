import React, { useEffect } from 'react';
import { DoorConfig } from './ScaderConfig';
import { ScaderScreenProps } from './ScaderCommon';
import { LockedIcon, UnlockedIcon } from './ScaderIcons';
import { ScaderManager } from './ScaderManager';
import { ScaderDoorStates, ScaderState } from './ScaderState';

const scaderManager = ScaderManager.getInstance();

export default function ScaderDoors(props:ScaderScreenProps) {

  const scaderConfigName = "ScaderDoors";
  const scaderStateName = "ScaderDoors";
  const configElemsName = "elems";
  const stateElemsName = "elems";
  const subElemsFriendly = "doors";
  const subElemsFriendlyCaps = "Door";
  const restCommandName = "door";
  const [config, setConfig] = React.useState(props.config[scaderConfigName]);
  const [state, setState] = React.useState(new ScaderState()[scaderStateName]);

  useEffect(() => {
    scaderManager.onConfigChange((newConfig) => {
      console.log(`${scaderConfigName}.onConfigChange`);
      // Update config
      setConfig(newConfig[scaderConfigName]);
    });
    scaderManager.onStateChange(scaderStateName, (newState) => {
      console.log(`${scaderStateName} onStateChange ${JSON.stringify(newState)}`);
      // Update state
      if (stateElemsName in newState) {
        setState(new ScaderDoorStates(newState));
      }
    });
  }, []);

  const updateMutableConfig = (newConfig: any) => {
    // Update ScaderManager
    scaderManager.getMutableConfig()[scaderConfigName] = newConfig;
  }

  const handleEnableChange = (event: React.ChangeEvent<HTMLInputElement>) => {
    console.log(`${scaderConfigName}.handleEnableChange`);
    // Update config
    const newConfig = {...config, enable: event.target.checked};
    setConfig(newConfig);
    updateMutableConfig(newConfig);
  };

  const handleNumElemsChange = (event: React.ChangeEvent<HTMLInputElement>) => {
    console.log(`${scaderConfigName}.handleNumElemsChange ${event.target.value}`);
    // Update config
    const newConfig = {...config};
    if (config[configElemsName].length < Number(event.target.value)) {
      // Add elements
      console.log(`${scaderConfigName}.handleNumElemsChange add ${Number(event.target.value) - config[configElemsName].length} elems`);
      let newElems:Array<DoorConfig> = [];
      for (let i = config[configElemsName].length; i < Number(event.target.value); i++) {
        newElems.push({name: `${subElemsFriendlyCaps} ${i+1}`});
      }
      newConfig[configElemsName].push(...newElems);
    } else {
      // Remove elems
      newConfig[configElemsName].splice(Number(event.target.value), config[configElemsName].length - Number(event.target.value));
    }
    setConfig(newConfig);
    updateMutableConfig(newConfig);
  };

  const handleElemNameChange = (event: React.ChangeEvent<HTMLInputElement>) => {
    console.log(`${scaderConfigName}.handleElemNameChange ${event.target.id} = ${event.target.value}`);
    // Update config
    const newConfig = {...config};
    let elemIndex = Number(event.target.id.split("-")[1]);
    newConfig[configElemsName][elemIndex].name = event.target.value;
    setConfig(newConfig);
    updateMutableConfig(newConfig);
    console.log(`${scaderConfigName}.handleElemNameChange ${JSON.stringify(newConfig)}`);
  };

  const handleElemClick = (event: React.MouseEvent<HTMLButtonElement>) => {
    console.log(`${scaderConfigName}.handleElemClick ${event.currentTarget.id}`);
    // Send command to change elem state
    let elemIndex = Number(event.currentTarget.id.split("-")[1]);
    scaderManager.sendCommand(`/${restCommandName}/${elemIndex+1}/unlock`);
  };

  const editModeScreen = () => {
    return (
      <div className="ScaderElem-edit">
        <div className="ScaderElem-header">
          {/* Checkbox for enable with label */}
          <label>
            <input className="ScaderElem-checkbox" type="checkbox" 
                  checked={config.enable} 
                  onChange={handleEnableChange} />
            Enable {scaderConfigName}
          </label>
        </div>
        {config.enable && 
          <div className="ScaderElem-body">
            {/* Input spinner with number of elems */}
            <label>
              Number of {subElemsFriendly}:
              <input className="ScaderElem-input" type="number" 
                  value={config[configElemsName].length} min="1" max={config.maxElems ? config.maxElems : 24}
                  onChange={handleNumElemsChange} />
            </label>
            {/* Input text for each elem name */}
            {Array.from(Array(config[configElemsName].length).keys()).map((index) => (
              <div className="ScaderElem-row" key={index}>
                <label key={index}>
                  {subElemsFriendlyCaps} {index+1} name:
                  <input className="ScaderElem-input" type="text" 
                      id={`${configElemsName}-${index}`}
                      value={config[configElemsName][index].name} 
                      onChange={handleElemNameChange} />
                </label>
              </div>
            ))}
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
            {/* Grid of elements */}
            {config[configElemsName].map((elem, index) => (
              <button key={index} className="ScaderElem-button" 
                      onClick={handleElemClick}
                      id={`${configElemsName}-${index}`}>
                  <div className="ScaderElem-button-text">{elem.name}</div>
                  {state[stateElemsName] && 
                        state[stateElemsName][index] &&
                        state[stateElemsName][index].open === 'Y' ?
                        <div className="ScaderElem-isOpen">Open</div> : ""}
                  {/* Display on icon if the state is on, else off icon */}
                  <div className="ScaderElem-button-icon">
                    {state[stateElemsName] && 
                          state[stateElemsName][index] &&
                          state[stateElemsName][index].locked === 'N' ? 
                            <UnlockedIcon fill="red" /> : 
                            <LockedIcon fill="white"/>}
                  </div>
              </button>
            ))}
            {/* Bell */}
            {state["bell"] && state["bell"] === 'Y' ? <div className="ScaderElem-bell-text">Doorbell is Ringing</div> : null}
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
