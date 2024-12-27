import React, { useEffect } from 'react';
import { ElecMeterConfig, ScaderConfig } from './ScaderConfig';
import { ScaderScreenProps } from './ScaderCommon';
import { ScaderManager } from './ScaderManager';
import { ScaderElecMeterStates, ScaderState } from './ScaderState';

const scaderManager = ScaderManager.getInstance();
type ScaderElecMetersConfig = ScaderConfig['ScaderElecMeters'];

export default function ScaderElecMeters(props:ScaderScreenProps) {

  const scaderName = "ScaderElecMeters";
  const configElemsName = "elems";
  const stateElemsName = "elems";
  const subElemsFriendly = "ctclamps";
  const subElemsFriendlyCaps = "CTClamp";
  const restCommandName = "elecmeter/value";
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
        setState(new ScaderElecMeterStates(newState));
      }
    });
  }, []);

  const updateMutableConfig = (newConfig: ScaderElecMetersConfig) => {
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
      const newElems:Array<ElecMeterConfig> = [];
      for (let i = config[configElemsName].length; i < Number(event.target.value); i++) {
        newElems.push({name: `${subElemsFriendlyCaps} ${i+1}`, calibADCToAmps: 0.05});
      }
      newConfig[configElemsName].push(...newElems);
    } else {
      // Remove elems
      newConfig[configElemsName].splice(Number(event.target.value), config[configElemsName].length - Number(event.target.value));
    }
    setConfig(newConfig);
    updateMutableConfig(newConfig);
  };

  const handleElemConfigChange = (event: React.ChangeEvent<HTMLInputElement>) => {
    console.log(`${scaderName}.handleElemConfigChange ${event.target.id} = ${event.target.value}`);
    // Update config
    const newConfig = {...config};
    const elemIndex = Number(event.target.id.split("-")[1]);
    // Get the value for the element name using the id of the input element `${configElemsName}-name-${index}`
    const nameElem = document.getElementById(`${configElemsName}_name-${elemIndex}`) as HTMLInputElement;
    if (nameElem) {
      newConfig[configElemsName][elemIndex].name = nameElem.value;
    }
    // Get the value for the element calibration using the id of the input element `${configElemsName}-calibADCToAmps-${index}`
    const calibADCToAmpsElem = document.getElementById(`${configElemsName}_calibADCToAmps-${elemIndex}`) as HTMLInputElement;
    if (calibADCToAmpsElem) {
      newConfig[configElemsName][elemIndex].calibADCToAmps = Number(calibADCToAmpsElem.value);
    }
    setConfig(newConfig);
    updateMutableConfig(newConfig);
    console.log(`${scaderName}.handleElemConfigChange ${JSON.stringify(newConfig)}`);
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
            {/* Mains voltage to use */}
            <label>
              Mains Voltage:
              <input className="ScaderElem-input" type="number" 
                  value={config.mainsVoltage} 
                  onChange={(event) => {
                    const newConfig = {...config, mainsVoltage: Number(event.target.value)};
                    setConfig(newConfig);
                    updateMutableConfig(newConfig);
                  }} />
            </label>
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
                      id={`${configElemsName}_name-${index}`}
                      value={config[configElemsName][index].name} 
                      onChange={handleElemConfigChange} />
                </label>
                {/* Input box for Current Transformer calibration value ADC -> Current */}
                <label key={index}>
                  CT Calibration:
                  <input className="ScaderElem-input" type="number" 
                      onChange={handleElemConfigChange}
                      id={`${configElemsName}calibADCToAmps-${index}`}
                      value={config[configElemsName][index].calibADCToAmps ? config[configElemsName][index].calibADCToAmps : 0.05} 
                      />
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
            <div className="ScaderElem-status-grid5">
              {/* Grid of elements */}
              {config[configElemsName].map((elem, index) => (
                <>
                  <div key={index}>
                    {elem.name}
                  </div>
                  <div key={index+1000}>
                    {state?.[stateElemsName]?.[index]?.rmsCurrentA + "A" || ""}
                  </div>
                  <div key={index+2000}>
                    {state?.[stateElemsName]?.[index]?.rmsPowerW + "W" || ""}
                  </div>
                  <div key={index+3000}>
                    {state?.[stateElemsName]?.[index]?.totalKWh + "kWh" || ""}
                  </div>
                  <div key={index+4000}>
                    <input className='ScaderElem-input-inline' type="number" id={`elem-${index}-kwh-input`} />
                    <button className='ScaderElem-button-inline' onClick={() => {
                          // Send rest command to set the total kWh
                          const kwh = document.getElementById(`elem-${index}-kwh-input`) as HTMLInputElement;
                          if (kwh) {
                            scaderManager.sendCommand(`/${restCommandName}/${index+1}/${kwh.value}`);
                          }
                        }}>
                      Set
                    </button>
                  </div>
                </>
              ))}
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
