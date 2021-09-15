local protoHelper = {}

function protoHelper.setCtrlOptArgs(fields)
  ctrlArgs = ""

  if fields.filterLogs.value then
    ctrlArgs = ctrlArgs .. " --applyLogFilter"
  end

  if fields.colorizeStderrOutput.value then
    ctrlArgs = ctrlArgs .. " --colorizeStderrOutput"
  end

  if fields.isWhiskey.value then
    ctrlArgs = ctrlArgs .. " --whiskey"
  end

  return ctrlArgs
end

return protoHelper