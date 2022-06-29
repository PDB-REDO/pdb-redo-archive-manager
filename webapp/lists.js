import "regenerator-runtime/runtime";
import 'bootstrap';

const _singleton = Symbol();
const PAGE_SIZE = 10;

class Pager {
	constructor(singletonToken) {
		if (_singleton !== singletonToken)
			throw new Error('Cannot instantiate directly');

		this.ul = document.getElementById('stable-pager');
		this.tbl = document.getElementById('entries-table');

		this.buttons = [...this.ul.querySelectorAll('li')];
		this.pageButtonCount = this.buttons.length - 4;
		this.halfPageButtonCount = Math.round((this.pageButtonCount - 1) / 2);

		this.buttons.forEach(btn => {
			btn.addEventListener('click', () => {
				if (btn.getAttribute('data-page-id') !== undefined)
					this.turnPage(btn);
			});
		});
	}

	static get instance() {
		if (this[_singleton] == undefined)
			this[_singleton] = new Pager(_singleton);
		
		return this[_singleton];
	}
	
	setQuery(q, entryCount) {
		this.q = JSON.stringify(q);

		let lastPage = Math.round(entryCount / PAGE_SIZE);
		if (lastPage * PAGE_SIZE < entryCount)
			lastPage += 1;

		this.page = 1;
		this.lastPage = lastPage;
		this.pageSize = PAGE_SIZE;

		this.updateButtons();
		this.selectPage(1);
	}

	turnPage(btn) {
		let page = this.page;

		const pageID = btn.getAttribute('data-page-id');
		switch (pageID) {
			case 'previous':
				page = page - 1;
				break;
			case 'next':
				page = page + 1;
				break;
			default:
				page = parseInt(btn.textContent);
				break;
		}
		if (page > this.lastPage)
			page = this.lastPage;
		if (page < 1)
			page = 1;

		this.selectPage(page);
	}

	selectPage(page) {
		fetch(`./entries-table?page=${page - 1}`, {
			credentials: "include",
			method: "post",
			body: this.q
		}).then(reply => {
			if (reply.ok)
				return reply.text();
			throw 'failed to fetch table';
		}).then(table => {
			this.tbl.innerHTML = table;
			this.page = page;
			this.updateButtons();
		}).catch(err => {
			console.log(err);
		});
	}

	updateButtons() {
		let pageForBtn1 = this.page - this.halfPageButtonCount;
		if (pageForBtn1 > this.lastPage - this.pageButtonCount)
			pageForBtn1 = this.lastPage - this.pageButtonCount;
		if (pageForBtn1 < 1)
			pageForBtn1 = 1;

		this.buttons[0].classList.toggle('disabled', this.page <= 1);
		this.buttons[1].style.display = pageForBtn1 > 1 ? '' : 'none';
	
		for (let i = 2; i < this.buttons.length - 2; ++i) {
			const pageNr = pageForBtn1 + i - 2;
			const btn = this.buttons[i];
			const a = btn.querySelector('a');
			a.textContent = pageNr;
			btn.classList.toggle('active', pageNr == this.page);
			btn.classList.toggle('d-none', pageNr > this.lastPage)
		}
	
		this.buttons[this.buttons.length - 2].style.display = pageForBtn1 < this.lastPage - this.pageButtonCount ? '' : 'none';
		this.buttons[this.buttons.length - 1].classList.toggle('disabled', this.page >= this.lastPage);
	}
}

export default Pager;