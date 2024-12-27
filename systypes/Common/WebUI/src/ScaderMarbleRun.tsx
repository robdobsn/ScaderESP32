import React, { useEffect } from 'react';
import { ScaderScreenProps } from './ScaderCommon';
import { ScaderManager } from './ScaderManager';
import { ScaderConfig } from './ScaderConfig';

const scaderManager = ScaderManager.getInstance();
type ScaderMarbleRunConfig = ScaderConfig['ScaderMarbleRun'];

export default function ScaderMarbleRun(props: ScaderScreenProps) {
  const scaderName = 'ScaderMarbleRun';
  const [config, setConfig] = React.useState<ScaderMarbleRunConfig>(props.config[scaderName]);
  const [mutableConfig, setMutableConfig] = React.useState<ScaderMarbleRunConfig>({ ...props.config[scaderName] });

  useEffect(() => {
    scaderManager.onConfigChange((newConfig) => {
      console.log(`${scaderName}.onConfigChange`);
      setConfig(newConfig[scaderName]);
    });
  }, []);

  useEffect(() => {
    // Sync mutableConfig with props.config when the parent updates it
    setMutableConfig({ ...props.config[scaderName] });
  }, [props.config]);

  const updateMutableConfig = (newConfig: ScaderMarbleRunConfig) => {
    scaderManager.getMutableConfig()[scaderName] = newConfig;
  };

  const handleEnableChange = (event: React.ChangeEvent<HTMLInputElement>) => {
    const newConfig = { ...mutableConfig, enable: event.target.checked };
    setMutableConfig(newConfig);
    updateMutableConfig(newConfig);
  };

  const handleSpeedChange = (event: React.ChangeEvent<HTMLInputElement>) => {
    const newConfig = { ...mutableConfig, speedPC: parseInt(event.target.value, 10) };
    setMutableConfig(newConfig);
    updateMutableConfig(newConfig);
  };

  const handleDurationChange = (event: React.ChangeEvent<HTMLInputElement>) => {
    const newConfig = { ...mutableConfig, durationMins: parseInt(event.target.value, 10) };
    setMutableConfig(newConfig);
    updateMutableConfig(newConfig);
  };

  const handleRunClick = async () => {
    try {
      const params = new URLSearchParams();
      params.append('speed', mutableConfig.speedPC.toString());
      params.append('duration', mutableConfig.durationMins.toString());

      console.log(`${scaderName}: Sending run command with params ${params.toString()}`);

      const response = await fetch(`/api/marbles/run?${params.toString()}`, { method: 'GET' });

      if (response.ok) {
        console.log(`${scaderName}: Run command sent successfully`);
      } else {
        console.error(`${scaderName}: Failed to send run command, status ${response.status}`);
      }
    } catch (error) {
      console.error(`${scaderName}: Error sending run command`, error);
    }
  };

  const handleStopClick = async () => {
    try {
      const response = await fetch(`/api/marbles/stop`, { method: 'GET' });

      console.log(`${scaderName}: Sending stop command`);

      if (response.ok) {
        console.log(`${scaderName}: Stop command sent successfully`);
      } else {
        console.error(`${scaderName}: Failed to send stop command, status ${response.status}`);
      }
    } catch (error) {
      console.error(`${scaderName}: Error sending stop command`, error);
    }
  };

  const editModeScreen = () => (
    <div className="ScaderElem-edit">
      <div className="ScaderElem-editmode">
        <label>
          <input
            className="ScaderElem-checkbox"
            type="checkbox"
            checked={mutableConfig.enable}
            onChange={handleEnableChange}
          />
          Enable {scaderName}
        </label>
        <div>
          <label>
            Speed (%):
            <input
              type="number"
              min="10"
              max="500"
              value={mutableConfig.speedPC}
              onChange={handleSpeedChange}
            />
          </label>
        </div>
        <div>
          <label>
            Duration (minutes):
            <input
              type="number"
              min="1"
              value={mutableConfig.durationMins}
              onChange={handleDurationChange}
            />
          </label>
        </div>
      </div>
    </div>
  );

  const normalModeScreen = () => (
    config.enable ? (
      <div className="ScaderElem">
        <div className="ScaderElem-header">
          <p>Speed: {config.speedPC}%</p>
          <p>Duration: {config.durationMins} minutes</p>
        </div>
        <div>
          <button className="ScaderElem-button button-medium" onClick={handleRunClick}>
            Run
          </button>
          <button className="ScaderElem-button button-medium" onClick={handleStopClick}>
            Stop
          </button>
        </div>
      </div>
    ) : null
  );

  return props.isEditingMode ? editModeScreen() : normalModeScreen();
}
