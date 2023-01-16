import React, { useEffect } from 'react';
import { ScaderScreenProps } from './ScaderCommon';
import { ScaderManager } from './ScaderManager';
import { ScaderOpenerStates, ScaderState } from './ScaderState';

const scaderManager = ScaderManager.getInstance();

export default function ScaderOpener(props:ScaderScreenProps) {

  const scaderConfigName = "ScaderOpener";
  const scaderStateName = "ScaderOpener";
  const stateElemsName = "status";
  const restCommandName = "opener";
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
        setState(new ScaderOpenerStates(newState));
      }
    });
  }, []);

  const updateConfigValue = (key: string, value: string | boolean | number | object) => {
    // Update config
    const newConfig = {...config, [key]: value};
    setConfig(newConfig);
    updateMutableConfig(newConfig);
  }

  const updateMutableConfig = (newConfig: any) => {
    // Update ScaderManager
    scaderManager.getMutableConfig()[scaderConfigName] = newConfig;
  }

  const handleEnableChange = (event: React.ChangeEvent<HTMLInputElement>) => {
    console.log(`${scaderConfigName}.handleEnableChange`);
    updateConfigValue("enable", event.target.checked);
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
            Enable {scaderConfigName}
          </label>
        </div>
        {config.enable && state.status && 
          <div className="ScaderElem-editmode">
            {/* Status info */}
            <div className="ScaderElem-status">
              {state.status.isOpen ? <div>OPEN</div> : <div>CLOSED</div>}
              {state.status.inEnabled ? <div>INEn</div> : null}
              {state.status.outEnabled ? <div>OUTEn</div> : null}
              {state.status.isOverCurrent ? <div>OverCurrent</div> : null}
              <div>ButSt: {state.status.kitchenButtonState.toString()}</div>
              {state.status.consButtonPressed ? <div>CONSBut</div> : null}
              {state.status.pirSenseInActive ? <div>PIR_IN</div> : null}
              {state.status.pirSenseOutActive ? <div>PIR_OUT</div> : null}
              <div>AvgI: {state.status.avgCurrent.toString()}</div>
            </div>

            <div className="ScaderElem-edit-group">
              <div className="ScaderElem-edit-line">
                {/* Numeric input box and label for number of degrees to turn to */}
                <label>
                  Angle (Degrees):
                  <input id="scader-motor-num-degrees" className="ScaderElem-input-small" type="number" />
                </label>
                {/* Button to turn motor getting degrees from input */}
                <button className="ScaderElem-button-editmode" onClick={
                      () => {
                        const inputElem = document.getElementById("scader-motor-num-degrees") as HTMLInputElement | null;
                        const numDegrees = inputElem ? inputElem.value : 0;
                        scaderManager.sendCommand(`/${restCommandName}/test/turnto/${numDegrees}`)
                      }
                      }>
                  Turn Motor to Angle
                </button>
                {/* Button to set current position as open */}
                <button className="ScaderElem-button-editmode" onClick={
                      () => {
                        const inputElem = document.getElementById("scader-motor-num-degrees") as HTMLInputElement | null;
                        const numDegrees = inputElem ? inputElem.value : 0;
                        updateConfigValue("DoorOpenAngle", numDegrees);
                        // scaderManager.sendCommand(`/${restCommandName}/setopenangle/${numDegrees}`);
                      }}>
                  Set Open Position
                </button>
              </div>

              <div className="ScaderElem-edit-line">
                {/* Numeric input box and label for time for motor to remain on after movement */}
                <label>
                  Motor on time after move (secs):
                  <input id="scader-motor-on-time-secs" className="ScaderElem-input-small" type="number" />
                </label>
                {/* Button to turn motor getting degrees from input */}
                <button className="ScaderElem-button-editmode" onClick={
                      () => {
                        const inputElem = document.getElementById("scader-motor-on-time-secs") as HTMLInputElement | null;
                        const numSecs = inputElem ? inputElem.value : 0;
                        updateConfigValue("MotorOnTimeAfterMoveSecs", numSecs);
                        // scaderManager.sendCommand(`/${restCommandName}/setmotorontime/${numSecs}`)
                      }
                      }>
                  Set
                </button>
              </div>

              <div className="ScaderElem-edit-line">
                {/* Numeric input box and label for door open time */}
                <label>
                  Door open time (secs):
                  <input id="scader-door-open-time-secs" className="ScaderElem-input-small" type="number" />
                </label>
                {/* Button to turn motor getting degrees from input */}
                <button className="ScaderElem-button-editmode" onClick={
                      () => {
                        const inputElem = document.getElementById("scader-door-open-time-secs") as HTMLInputElement | null;
                        const numSecs = inputElem ? inputElem.value : 0;
                        updateConfigValue("DoorOpenTimeSecs", numSecs);
                        // scaderManager.sendCommand(`/${restCommandName}/setdooropentime/${numSecs}`)
                      }
                      }>
                  Set
                </button>
              </div>

              <div className="ScaderElem-edit-line">
                {/* Numeric input box and label for motor current limit */}
                <label>
                  Motor current limit (A):
                  <input id="scader-motor-current-limit" className="ScaderElem-input-small" type="number" />
                </label>
                {/* Button to set motor current limit */}
                <button className="ScaderElem-button-editmode" onClick={
                      () => {
                        const inputElem = document.getElementById("scader-motor-current-limit") as HTMLInputElement | null;
                        const numAmps = inputElem ? inputElem.value : 0;
                        const currentVal = {axes: [{A: {driver: {rmsAmps: numAmps}}}]}
                        updateConfigValue("stepper", currentVal);
                      }
                      }>
                  Set
                </button>
              </div>

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
            Hello!
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
