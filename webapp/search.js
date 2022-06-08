
export function construct_query() {
	const programSelector = document.getElementById('inputProgramName');
	const versionSelector = document.getElementById('inputProgramVersion');

	return `program=${encodeURIComponent(programSelector.value)}&version=${encodeURIComponent(versionSelector.value)}`;
}

function updateVersionSelector() {

	const programSelector = document.getElementById('inputProgramName');
	const versionSelector = document.getElementById('inputProgramVersion');
	
	while (versionSelector.length > 0)
		versionSelector.remove(0);

	for (const sw of SOFTWARE) {
		if (sw.name != programSelector.value)
			continue;

		for (const version of sw.versions) {
			const opt = document.createElement("option");
			opt.text = version;
			versionSelector.add(opt);
		}

		break;
	}
}

window.addEventListener('load', () => {

	const programSelector = document.getElementById('inputProgramName');
	const versionSelector = document.getElementById('inputProgramVersion');

	if (programSelector && versionSelector) {
		programSelector.addEventListener('change', updateVersionSelector);
	}
})