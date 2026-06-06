% Wrapper to run batch validation with a stable EIDORS path setup.
% Avoids shell quoting issues in VS Code tasks on PowerShell.

eidors_dir = getenv('EIDORS_DIR');
if isempty(eidors_dir)
    eidors_dir = 'C:/Users/g_kol/Downloads/eidors-v3.12-ng/eidors-v3.12-ng/eidors';
    setenv('EIDORS_DIR', eidors_dir);
end

fprintf('run_batch_validate: EIDORS_DIR=%s\n', getenv('EIDORS_DIR'));

batch_validate;
