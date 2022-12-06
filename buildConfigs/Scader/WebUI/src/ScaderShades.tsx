import React, { useEffect } from 'react';
import { ShadesConfig } from './ScaderConfig';
import { ScaderScreenProps } from './ScaderCommon';
import { DownIcon, StopIcon, UpIcon } from './ScaderIcons';
import { ScaderManager } from './ScaderManager';
import { ScaderState } from './ScaderState';

const scaderManager = ScaderManager.getInstance();

function ScaderShades(props:ScaderScreenProps) {

  const scaderName = "ScaderShades";
  const configElemsName = "elems";
  const stateElemsName = "elems";
  const subElemsFriendly = "shades";
  const subElemsFriendlyCaps = "Shade";
  const restCommandName = "shade";
  const [config, setConfig] = React.useState(props.config[scaderName]);
  const [state, setState] = React.useState(new ScaderState()[scaderName]);

  useEffect(() => {
    scaderManager.onConfigChange((newConfig) => {
      console.log(`${scaderName}.onConfigChange`);
      // Update config
      setConfig(newConfig[scaderName]);
    });
    scaderManager.onStateChange((newState) => {
      console.log(`${scaderName}onStateChange`);
      // Update state
      if (stateElemsName in newState) {
        setState(newState);
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

  const handleNumElemsChange = (event: React.ChangeEvent<HTMLInputElement>) => {
    console.log(`${scaderName}.handleNumElemsChange ${event.target.value}`);
    // Update config
    const newConfig = {...config};
    if (config[configElemsName].length < Number(event.target.value)) {
      // Add elements
      console.log(`${scaderName}.handleNumElemsChange add ${Number(event.target.value) - config[configElemsName].length} elems`);
      let newElems:Array<ShadesConfig> = [];
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
    console.log(`${scaderName}.handleElemNameChange ${event.target.id} = ${event.target.value}`);
    // Update config
    const newConfig = {...config};
    let elemIndex = Number(event.target.id.split("-")[1]);
    newConfig[configElemsName][elemIndex].name = event.target.value;
    setConfig(newConfig);
    updateMutableConfig(newConfig);
    console.log(`${scaderName}.handleElemNameChange ${JSON.stringify(newConfig)}`);
  };

  const handleElemClick = (event: React.MouseEvent<HTMLButtonElement>) => {
    console.log(`${scaderName}.handleElemClick ${event.currentTarget.id}`);
    // Send command to change elem state
    const splitId = event.currentTarget.id.split("-");
    const elemIndex = Number(splitId[1]);
    const cmdName = splitId[2];
    scaderManager.sendCommand(`/${restCommandName}/${elemIndex}/${cmdName}/pulse`);
  };

  const editModeScreen = () => {
    return (
      <div className="ScaderElem">
        <div className="ScaderElem-header">
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
          <header className="ScaderElem-header">
            {/* Grid of elements */}
            <div className="ScaderElem-gridhoriz">
              {config[configElemsName].map((elem, index) => (
                <div className="ScaderElem-shadegroup" key={index} >
                  <div className="ScaderElem-shadename">{elem.name}</div>
                  <button key={index*50+10} className="ScaderElem-button" 
                          onClick={handleElemClick}
                          id={`${subElemsFriendly}-${index}-up`}>
                      {<UpIcon fill="#ffffff" />}
                  </button>
                  <button key={index*50+11} className="ScaderElem-button" 
                          onClick={handleElemClick}
                          id={`${subElemsFriendly}-${index}-stop`}>
                      {<StopIcon fill="#ffffff" />}
                  </button>
                  <button key={index*50+12} className="ScaderElem-button" 
                          onClick={handleElemClick}
                          id={`${subElemsFriendly}-${index}-down`}>
                      {<DownIcon fill="#ffffff" />}
                  </button>
                </div>
              ))}
            </div>
          </header>
        </div>
      : null
    )
  }

  return (
    // Show appropriate screen based on mode
    props.isEditingMode ? editModeScreen() : normalModeScreen()
  );
}

export default ScaderShades;