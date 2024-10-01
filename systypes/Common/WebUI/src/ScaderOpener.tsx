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
    console.log(`updateConfigValue beforeChange key ${key} value ${value} ${JSON.stringify(config)}`);
    const newConfig = {...config, [key]: value};
    delete newConfig.doorSwingAngleDegrees;
    delete newConfig.doorClosedAngleOffsetDegrees;
    delete newConfig.DoorClosedAngle;
    delete newConfig.DoorOpenAngle;
    setConfig(newConfig);
    updateMutableConfig(newConfig);
    console.log(`updateConfigValue afterChange key ${key} value ${value} ${JSON.stringify(config)}`);
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
              {state.status.inEnabled ? <div className="Scader-enabled">IN</div> : <div className="Scader-disabled">IN</div>}
              {state.status.outEnabled ? <div className="Scader-enabled">OUT</div> : <div className="Scader-disabled">OUT</div>}
              {state.status.kitButtonPressed ? <div>KitBut</div> : null}
              {state.status.consButtonPressed ? <div>CONSBut</div> : null}
              {state.status.pirSenseInActive ? <div>PIR_IN</div> : null}
              {state.status.pirSenseOutActive ? <div>PIR_OUT</div> : null}
              <div>{"Angle: " + state.status.doorCurAngle.toFixed(1) + "°"}</div>
              <div>{"Closed: " + state.status.doorClosedAngleDegs.toFixed(1) + "°"}</div>
              <div>{"Open: " + state.status.doorOpenAngleDegs.toFixed(1) + "°"}</div>
              <div>{"Raw Force: " + state.status.rawForceN.toFixed(1) + "N"}</div>
              <div>{"Measured Force: " + state.status.measuredForceN.toFixed(1) + "N"}</div>
            </div>
          </div>
        }
        {config.enable &&
          <div className="ScaderElem-editmode">
            <div className="ScaderElem-edit-group">
              {/* <div className="ScaderElem-edit-line">
                <button className="ScaderElem-button-editmode" onClick={
                      () => {
                        scaderManager.sendCommand(`/${restCommandName}/calibrate`)
                      }
                    }>
                  Calibrate (assume door closed initially)
                </button>
              </div> */}
              {/* <div className="ScaderElem-edit-line">
                <p>Note: Closed position must be set first AND both sensor and stepper rotation must be increasingly positive as door is opening</p>
              </div> */}
              <div className="ScaderElem-edit-line">
                {/* Button to set closed position */}
                <button className="ScaderElem-button-editmode" onClick={
                      () => {
                        console.log(`curAngleDegs ${state.status.doorCurAngle} config ${JSON.stringify(config)}`);
                        updateConfigValue("DoorClosedAngleDegs", state.status.doorCurAngle);
                      }}>
                  Set Closed Position
                </button>
                {/* Button to set open position */}
                <button className="ScaderElem-button-editmode" onClick={
                      () => {
                        updateConfigValue("DoorOpenAngleDegs", state.status.doorCurAngle);
                      }}>
                  Set Open Position
                </button>
                {/* Button to set force offset */}
                <button className="ScaderElem-button-editmode" onClick={
                      () => {
                        updateConfigValue("ForceOffsetN", state.status.rawForceN);
                      }}>
                  Set Force Offset
                </button>
              </div>
              <div className="ScaderElem-edit-line">
                {/* Numeric input box and label for number of degrees to turn to */}
                <label>
                  Angle (degs):
                  <input id="scader-motor-num-degrees" className="ScaderElem-input-small" type="number" defaultValue="30" />
                </label>
                {/* Button to turn motor getting degrees from input */}
                <button className="ScaderElem-button-editmode" onClick={
                      () => {
                        const inputElem = document.getElementById("scader-motor-num-degrees") as HTMLInputElement | null;
                        const numDegrees = inputElem ? inputElem.value : 0;
                        scaderManager.sendCommand(`/${restCommandName}/test/turnto/${numDegrees}`)
                      }
                      }>
                  Turn
                </button>
              </div>

              <div className="ScaderElem-edit-line">
                {/* Numeric input box and label for time for door to move to open/closed */}
                <label>
                  Time to open (s):
                  <input id="scader-door-time-to-open-secs" className="ScaderElem-input-small" type="number" defaultValue={config.DoorTimeToOpenSecs} />
                </label>
                <button className="ScaderElem-button-editmode" onClick={
                      () => {
                        const inputElem = document.getElementById("scader-door-time-to-open-secs") as HTMLInputElement | null;
                        const numSecs = inputElem ? inputElem.value : 0;
                        updateConfigValue("DoorTimeToOpenSecs", numSecs);
                      }
                      }>
                  Set
                </button>
              </div>              

              <div className="ScaderElem-edit-line">
                {/* Numeric input box and label for time for motor to remain on after movement */}
                <label>
                  Motor hold (s):
                  <input id="scader-motor-on-time-secs" className="ScaderElem-input-small" type="number" defaultValue={config.MotorOnTimeAfterMoveSecs} />
                </label>
                {/* Button to turn motor getting degrees from input */}
                <button className="ScaderElem-button-editmode" onClick={
                      () => {
                        const inputElem = document.getElementById("scader-motor-on-time-secs") as HTMLInputElement | null;
                        const numSecs = inputElem ? inputElem.value : 0;
                        updateConfigValue("MotorOnTimeAfterMoveSecs", numSecs);
                      }
                      }>
                  Set
                </button>
              </div>

              <div className="ScaderElem-edit-line">
                {/* Numeric input box and label for door open time */}
                <label>
                  Stay open (s):
                  <input id="scader-door-remain-open-time-secs" className="ScaderElem-input-small" type="number" defaultValue={config.DoorRemainOpenTimeSecs} />
                </label>
                {/* Button to turn motor getting degrees from input */}
                <button className="ScaderElem-button-editmode" onClick={
                      () => {
                        const inputElem = document.getElementById("scader-door-remain-open-time-secs") as HTMLInputElement | null;
                        const numSecs = inputElem ? inputElem.value : 0;
                        updateConfigValue("DoorRemainOpenTimeSecs", numSecs);
                      }
                      }>
                  Set
                </button>
              </div>

              <div className="ScaderElem-edit-line">
                {/* Numeric input box and label for motor current limit */}
                <label>
                  Current limit (A):
                  <input id="scader-motor-current-limit" className="ScaderElem-input-small" type="number" defaultValue={config.MaxMotorCurrentAmps} />
                </label>
                {/* Button to set motor current limit */}
                <button className="ScaderElem-button-editmode" onClick={
                      () => {
                        const inputElem = document.getElementById("scader-motor-current-limit") as HTMLInputElement | null;
                        const numAmps = inputElem ? inputElem.value : 0;
                        updateConfigValue("MaxMotorCurrentAmps", numAmps);
                      }
                      }>
                  Set
                </button>
              </div>

              <div className="ScaderElem-edit-line">
                {/* Numeric input box and label for force threshold */}
                <label>
                  Force threshold (N):
                  <input id="scader-force-threshold" className="ScaderElem-input-small" type="number" defaultValue={config.ForceThresholdN} />
                </label>
                {/* Button to set force threshold */}
                <button className="ScaderElem-button-editmode" onClick={
                      () => {
                        const inputElem = document.getElementById("scader-force-threshold") as HTMLInputElement | null;
                        const newtons = inputElem ? inputElem.value : 0;
                        updateConfigValue("ForceThresholdN", newtons);
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

    // Out enabled button class
    let outEnabledButtonClass = "ScaderElem-button button-onoff " +
          (state.status && state.status.outEnabled ? "out-enabled" : "inout-disabled");
    // In enabled button class
    let inEnabledButtonClass = "ScaderElem-button button-onoff " +
          (state.status && state.status.inEnabled ? "in-enabled" : "inout-disabled");

    // Open state text
    let openState = state.status && state.status.doorStateStr;
    
    // (state.status && state.status.motorActive) ? "Closing" : "Closed";
    // if (state.status && (state.status.angleFromClosed > state.status.closedAngleTolerance)) {
    //   if (state.status.motorActive) {
    //     openState = "Opening";
    //   }
    //   else if (state.status.timeBeforeCloseSecs === 0) {
    //     openState = "Open indefinitely";
    //   } else {
    //     openState = `Open (for ${state.status.timeBeforeCloseSecs.toString()}s)`;
    //   }
    // }

    return (
      // Display if enabled
      config.enable ?
        <div className="ScaderElem">
          <div className="ScaderElem-section">
            {config.enable && state.status && 
            <div className="ScaderElem-status-grid">
              {/* Status info grid */}
              <div>{openState}</div>
              {state.status.kitButtonPressed ? <div>BUT-OUT</div> : null}
              {state.status.consButtonPressed ? <div>BUT-IN</div> : null}
              {state.status.pirSenseInActive ? <div>PIR-OUT</div> : null}
              {state.status.pirSenseOutActive ? <div>PIR-IN</div> : null}
              <div>{typeof state.status.doorCurAngle === "number" ? "Angle: " + state.status.doorCurAngle.toFixed(0) + "°" : ""}</div>
              <div>{typeof state.status.measuredForceN === "number" ? "Force: " + state.status.measuredForceN.toFixed(1) + "N" : ""}</div>
            </div>
          }
          </div>
          <div className="ScaderElem-section">
            {/* out-enabled button */}
            <button className={outEnabledButtonClass} onClick={
              () => {
                scaderManager.sendCommand(`/${restCommandName}/outenable/${state.status && state.status.outEnabled ? "0" : "1"}`)
              }
              }>
              {state.status && state.status.outEnabled ? "Out-Enabled" : "Out-Disabled"}
            </button>
            {/* in-enabled button */}
            <button className={inEnabledButtonClass} onClick={
              () => {
                scaderManager.sendCommand(`/${restCommandName}/inenable/${state.status && state.status.inEnabled ? "0" : "1"}`)
              }
              }>
              {state.status && state.status.inEnabled ? "In-Enabled" : "In-Disabled"}
            </button>
          </div>
          <div className="ScaderElem-section">
            {/* Button to open door */}
            <button className="ScaderElem-button button-medium" onClick={
              () => {
                scaderManager.sendCommand(`/${restCommandName}/open`)
              }
              }>
              Open
            </button>
            {/* Button to close door */}
            <button className="ScaderElem-button button-medium" onClick={
              () => {
                scaderManager.sendCommand(`/${restCommandName}/close`)
              }
              }>
              Close
            </button>
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
