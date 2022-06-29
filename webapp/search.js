import { post } from "jquery";
import Pager from "./lists";

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
	constructor(f) {
		this.form = f;

		f.addEventListener('submit', (e) => this.submit(e));
	}

	submit(e) {
		e.preventDefault();

		const query = this.constructQuery();

		const fd = new FormData();
		fd.append('query', JSON.stringify(query));

		fetch('/v1/q/count', {
			credentials: "include",
			method: "post",
			body: fd
		}).then(r => {
			if (r.ok)
				return r.json();
			throw 'failed to process query';
		}).then(data => {

			const counter = document.getElementById('entry-count-span');
			counter.textContent = data;

			const pager = Pager.instance;
			pager.setQuery(query, +data);
		}).catch(err => {
			console.log(err);
			alert('Failed to process query');
		});

		return false;
	}

	constructQuery() {
		let query = {
			latest: document.getElementById('latest-only-cb').checked,
			filters: []
		};
		
		const progFilters = document.querySelectorAll('div.prog-filter select');
		for (const f of progFilters) {
			const program = f.getAttribute('data-program');

			query.filters.push({
				"t": "sw",
				"s": program,
				"v": f.value,
				"o": "eq"
			});
		}

		const propFilters = document.querySelectorAll('div.prop-filter select');
		for (const f of propFilters) {
			const property = f.getAttribute('data-property');
			const v = f.parentNode.querySelector('input.prop-value');

			query.filters.push({
				"t": "d",
				"s": property,
				"o": f.value,
				"v": v.value
			});
		}


		console.log(query);

		return query;
	}
}

function createElement( str ) {
    var frag = document.createDocumentFragment();

    var elem = document.createElement('div');
    elem.innerHTML = str;

    while (elem.childNodes[0]) {
        frag.appendChild(elem.childNodes[0]);
    }
    return frag;
}



window.addEventListener('load', () => {
	const f = document.getElementById("query-form");

	new Query(f);

	const addBtnsDiv = document.getElementById('add-filter-buttons-div');

	const addProgramFilterBtn = document.getElementById('add-program-filter-btn');
	const addProgramFilterSelector = document.getElementById('add-program-filter-selector');

	addProgramFilterBtn.addEventListener('click', (e) => {
		const program = addProgramFilterSelector.value;
		fetch(`program-filter?program=${program}`, {
			credentials: "include"
		}).then(r => {
			if (r.ok)
				return r.text();
		}).then(filter => {
			f.insertBefore(createElement(filter), addBtnsDiv);
		});
	});

	const addPropertyFilterBtn = document.getElementById('add-property-filter-btn');
	const addPropertyFilterSelector = document.getElementById('add-property-filter-selector');

	addPropertyFilterBtn.addEventListener('click', (e) => {
		const property = addPropertyFilterSelector.value;
		fetch(`property-filter?property=${property}`, {
			credentials: "include"
		}).then(r => {
			if (r.ok)
				return r.text();
		}).then(filter => {
			f.insertBefore(createElement(filter), addBtnsDiv);
		});
	});


})