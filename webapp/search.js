
export function construct_query() {
	const programSelector = document.getElementById('inputProgramName');
	const versionSelector = document.getElementById('inputProgramVersion');

	// return `program=${encodeURIComponent(programSelector.value)}&version=${encodeURIComponent(versionSelector.value)}`;

	return JSON.stringify({
		latest: true,
		filters: [
			{
				t: 'sw',
				o: 'eq',
				s: 'BLASTp',
				v: '2.6.0+'
			},
			{
				t: 'd',
				o: 'eq',
				s: 'NBBFLIP',
				v: '0'
			}
		]
	});
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

class Query {
	constructor() {


	}

	submit() {

	}
}


window.addEventListener('load', () => {

	// const programSelector = document.getElementById('inputProgramName');
	// const versionSelector = document.getElementById('inputProgramVersion');

	// if (programSelector && versionSelector) {
	// 	programSelector.addEventListener('change', updateVersionSelector);
	// }

	// new Query();

	const f = document.getElementById("query-form");
	f.addEventListener('submit', () => {
		
	});


})