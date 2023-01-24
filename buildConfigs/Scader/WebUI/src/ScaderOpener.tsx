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
              <div>{state.status.inOutMode}</div>
              {state.status.isOverCurrent ? <div>OverCurrent</div> : null}
              {state.status.kitButtonPressed ? <div>KitBut</div> : null}
              {state.status.consButtonPressed ? <div>CONSBut</div> : null}
              {state.status.pirSenseInActive ? <div>PIR_IN</div> : null}
              {state.status.pirSenseOutActive ? <div>PIR_OUT</div> : null}
              <div>{state.status.rotationAngleDegs.toString() + "°"}</div>
              <div>{"Step:" + state.status.stepperCurAngle.toString() + "°"}</div>
            </div>
          </div>
        }
        {config.enable &&
          <div className="ScaderElem-editmode">
            <div className="ScaderElem-edit-group">
              <div className="ScaderElem-edit-line">
                {/* Button to set open position */}
                <button className="ScaderElem-button-editmode" onClick={
                      () => {
                        const numDegrees = state.status ? state.status.rotationAngleDegs : 0;
                        updateConfigValue("DoorOpenAngle", numDegrees);
                      }}>
                  Set Open Position
                </button>
                <button className="ScaderElem-button-editmode" onClick={
                      () => {
                        const numDegrees = state.status ? state.status.rotationAngleDegs : 0;
                        updateConfigValue("DoorClosedAngle", numDegrees);
                      }}>
                  Set Closed Position
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

            </div>
          </div>
        }
      </div>
    );
  }

  const normalModeScreen = () => {
    let inOutMode = "";
    if (state.status) {
      if (state.status.inEnabled && state.status.outEnabled) {
        inOutMode = "In/Out";
      } else if (state.status.inEnabled) {
        inOutMode = "In-Only";
      } else if (state.status.outEnabled) {
        inOutMode = "Out-Only";
      } else {
        inOutMode = "Locked";
      }
    }

    let openState = (state.status && state.status.isMoving) ? "Closing" : "Closed";
    if (state.status && state.status.isOpen) {
      if (state.status.isMoving) {
        openState = "Opening";
      }
      else if (state.status.timeBeforeCloseSecs === 0) {
        openState = "Open indefinitely";
      } else {
        openState = `Open (for ${state.status.timeBeforeCloseSecs.toString()}s)`;
      }
    }

    return (
      // Display if enabled
      config.enable ?
        <div className="ScaderElem">
          <div className="ScaderElem-body">
            {config.enable && state.status && 
            <div className="ScaderElem-status-grid">
              {/* Status info grid */}
              <div>{openState}</div>
              <div>{inOutMode}</div>
              {state.status.kitButtonPressed ? <div>Kitchen Button</div> : null}
              {state.status.consButtonPressed ? <div>Consv Button</div> : null}
              {state.status.pirSenseInActive ? <div>Kitchen PIR Actv</div> : null}
              {state.status.pirSenseInTriggered ? <div>Kitchen PIR Trig</div> : null}
              {state.status.pirSenseOutActive ? <div>Consv PIR Actv</div> : null}
              {state.status.pirSenseOutTriggered ? <div>Consv PIR Trig</div> : null}
              <div>{state.status.rotationAngleDegs ? state.status.rotationAngleDegs.toString() + "°" : ""}</div>
              <div>{state.status.stepperCurAngle ? "Step:" + state.status.stepperCurAngle.toString() + "°" : ""}</div>
            </div>
          }
          </div>
          <div className="ScaderElem-footer">
            {/* Button to open door */}
            <button className="ScaderElem-button" onClick={
              () => {
                scaderManager.sendCommand(`/${restCommandName}/open`)
              }
              }>
              Open
            </button>
            {/* Button to close door */}
            <button className="ScaderElem-button" onClick={
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
