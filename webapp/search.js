import Pager from "./lists";

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

		fetch('v1/q/count', {
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
			latest: true,
			filters: []
		};

		const latestSelect = document.getElementById('latest-only-select');
		query.latest = latestSelect.value == 'true';

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

			if (v === null) {	// boolean switch/value
				query.filters.push({
					"t": "d",
					"s": property,
					"o": "eq",
					"v": f.value == "true"
				});
			} else {
				query.filters.push({
					"t": "d",
					"s": property,
					"o": f.value,
					"v": v.value
				});
			}
		}

		// console.log(query);

		return query;
	}
}

function createElement(str) {
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
			const filterElem = createElement(filter);
			const filterId = filterElem.firstChild.id;
			const deleteFilterBtn = filterElem.querySelector('button.bi-trash');
			deleteFilterBtn.addEventListener('click', (e) => {
				e.preventDefault();

				const node = document.getElementById(filterId);
				f.removeChild(node);
			});
			f.insertBefore(filterElem, addBtnsDiv);
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
			const filterElem = createElement(filter);
			const filterId = filterElem.firstChild.id;
			const deleteFilterBtn = filterElem.querySelector('button.bi-trash');
			deleteFilterBtn.addEventListener('click', (e) => {
				e.preventDefault();

				const node = document.getElementById(filterId);
				f.removeChild(node);
			});
			f.insertBefore(filterElem, addBtnsDiv);
		});
	});

})