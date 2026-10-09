"""Static regression checks for scope of the target-ID-only build."""
from pathlib import Path

root = Path(__file__).resolve().parents[1]
controller = (root / 'generated_runtime/controller.cpp').read_text(encoding='utf-8')
bridge = (root / 'generated_runtime/bridge.cpp').read_text(encoding='utf-8')
cmake = (root / 'CMakeLists.txt').read_text(encoding='utf-8')

checks = {
    'existing account discovery and per-PID bridge': 'FindClients()' in controller and 'SetWindowsHookExW(' in controller,
    'account selection retained': 'LVS_EX_CHECKBOXES' in controller and 'IDC_START_CHECKED' in controller and 'IDC_STOP_CHECKED' in controller,
    'individual nearby scanner and checkbox': 'ScanNearbyPlayers' in controller and 'IDC_NEARBY_LIST' in controller and 'SetTargetFromCheck' in controller,
    'per-account target storage': 'AccountSection(int roleID)' in controller and 'TargetRoleID' in controller,
    'per-account workers and state': 'std::thread worker' in controller and 'target_loop::State loop' in controller,
    'manual click 1 and click 2 stored': all(s in controller for s in ('Click1X','Click1Y','Click2X','Click2Y','VK_F7','VK_F8')), 
    'target-trade-click loop actions': all('Command::'+cmd in controller for cmd in ('SelectTargetByRoleID','ClickTravelSemantic','ClickInternalPoint')) and 'Command::ClickTargetFace' not in controller,
    'per-account repeat and delay saved': all(key in controller for key in ('RepeatCycles','DelayTargetMs','DelayClick1Ms','DelayTradeMs','DelayClick2Ms','DelayCycleMs')),
    'log tab and logging': 'L"LOG"' in controller and 'PushLog(' in controller,
    'old workflow GUI removed': all('L"'+label+'"' not in controller for label in ('DỒN ĐỒ','TELEGRAM','DEVELOPER','AUTO TRAIN')),
    'legacy main/con worker removed': 'MAIN/CON' not in controller,
    'Windows build and portable tests': 'add_executable(ThanLongItemConsolidator WIN32' in cmake and 'target_loop_logic_tests' in cmake,
    'no obsolete modules in build': all(token not in cmake for token in ('route_logic_test.cpp','auto_loot_logic_test.cpp','telegram_logic_tests')),
}
for name, okay in checks.items():
    print(('PASS' if okay else 'FAIL') + ': ' + name)
if not all(checks.values()):
    raise SystemExit('Scope verification FAILED')
print(f'Scope verification PASSED: {len(checks)} checks')
