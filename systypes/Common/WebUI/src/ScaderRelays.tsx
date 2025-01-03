import React, { useEffect } from 'react';
import { RelayConfig, ScaderConfig } from './ScaderConfig';
import { ScaderScreenProps } from './ScaderCommon';
import { OnIcon } from './ScaderIcons';
import { ScaderManager } from './ScaderManager';
import { ScaderRelayStates, ScaderState } from './ScaderState';

const scaderManager = ScaderManager.getInstance();
type ScaderRelaysConfig = ScaderConfig['ScaderRelays'];

export default function ScaderRelays(props: ScaderScreenProps) {

  const scaderName = "ScaderRelays";
  const configElemsName = "elems";
  const stateElemsName = "elems";
  const subElemsFriendly = "relays";
  const subElemsFriendlyCaps = "Relay";
  const restCommandName = "relay";
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
      if (stateElemsName in newState) {
        setState(new ScaderRelayStates(newState as ScaderRelayStates));
      }
    });
  }, []);

  const updateMutableConfig = (newConfig: ScaderRelaysConfig) => {
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

  const handleNumElemsChange = (event: React.ChangeEvent<HTMLInputElement>) => {
    console.log(`${scaderName}.handleNumElemsChange ${event.target.value}`);
    // Update config
    const newConfig = { ...config };
    if (config[configElemsName].length < Number(event.target.value)) {
      // Add elements
      console.log(`${scaderName}.handleNumElemsChange add ${Number(event.target.value) - config[configElemsName].length} elems`);
      const newElems: Array<RelayConfig> = [];
      for (let i = config[configElemsName].length; i < Number(event.target.value); i++) {
        newElems.push({ name: `${subElemsFriendlyCaps} ${i + 1}`, isDimmable: false });
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
    const newConfig = { ...config };
    const elemIndex = Number(event.target.id.split("-")[1]);
    newConfig[configElemsName][elemIndex].name = event.target.value;
    setConfig(newConfig);
    updateMutableConfig(newConfig);
    console.log(`${scaderName}.handleElemNameChange ${JSON.stringify(newConfig)}`);
  };

  const handleElemDimmableChange = (event: React.ChangeEvent<HTMLInputElement>) => {
    console.log(`${scaderName}.handleElemDimmableChange ${event.target.id} = ${event.target.checked}`);
    // Update config
    const newConfig = { ...config };
    const elemIndex = Number(event.target.id.split("-")[1]);
    newConfig[configElemsName][elemIndex].isDimmable = event.target.checked;
    setConfig(newConfig);
    updateMutableConfig(newConfig);
    console.log(`${scaderName}.handleElemDimmableChange ${JSON.stringify(newConfig)}`);
  };

  const handleElemClick = (event: React.MouseEvent<HTMLButtonElement>) => {
    // Send command to change elem state
    const elemIndex = Number(event.currentTarget.id.split("-")[1]);
    const curState = state[stateElemsName]?.[elemIndex]?.state || 0;
    let newState = curState < 0 ? 0 : (curState < 100 ? curState : 100);
    if (config[configElemsName][elemIndex].isDimmable) {
      newState = curState + 10 > 100 ? 0 : curState + 10;
    } else {
      newState = curState === 0 ? 100 : 0;
    }
    const apiCmd = `/${restCommandName}/${elemIndex + 1}/${newState}`;
    scaderManager.sendCommand(apiCmd);
    console.log(`${scaderName}.handleElemClick ${event.currentTarget.id} cmd ${apiCmd}`);
  };

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
                  {subElemsFriendlyCaps} {index + 1} name:
                  <input className="ScaderElem-input" type="text"
                    id={`${configElemsName}-${index}`}
                    value={config[configElemsName][index].name}
                    onChange={handleElemNameChange} />
                </label>
                <label key={`${index}-dimmable`} style={{ marginLeft: '10px' }}>
                  <input type="checkbox"
                    id={`dimmable-${index}`}
                    checked={config[configElemsName][index].isDimmable || false}
                    onChange={handleElemDimmableChange} />
                  Dimmable
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
            {config[configElemsName].map((elem, index) => {
              const relayState = state[stateElemsName]?.[index]?.state || 0;
              const greyScaleValue = relayState == 0 ? 0 : 155 + Math.round((relayState / 100) * 100);
              const greyScaleColor = `rgb(${greyScaleValue}, ${greyScaleValue}, ${greyScaleValue})`;

              return (
                <button key={index} className="ScaderElem-button"
                  onClick={handleElemClick}
                  id={`${configElemsName}-${index}`}>
                  <div className="ScaderElem-button-text">{elem.name}</div>
                  <div className="ScaderElem-button-icon">
                    <OnIcon fill={greyScaleColor} />
                  </div>
                  <div className="ScaderElem-button-state">
                    {relayState}
                  </div>
                </button>
              );
            })}
          </div>
          {
            state.mainsHz ?
              <div className="ScaderElem-footer">
                Mains freq: {state.mainsHz} Hz
              </div>
              : null
          }
        </div>
        : null
    )
  }

  return (
    // Show appropriate screen based on mode
    props.isEditingMode ? editModeScreen() : normalModeScreen()
  );
}
