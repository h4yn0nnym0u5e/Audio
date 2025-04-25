/* public domain
 * vim: set ts=4:
 */

RED.storage = (function() {
	function target()
	{
		var result = "audio_library_guitool";
		var useLocation = true; // set false to return to old behaviour
		if (useLocation)
		{
			var url = window.location.href;
			// Use the changeable part of the URL, but
			// allow for "index-with-more-stuff.html"
			// files with different sets of objects
			var root = url.search(/gui[/]index.*[.]htm/);
			if (root >= 0)
			{
				root = url.indexOf(".htm")
				url = url.slice(0, root);
				result = url + "/" + result
			}
		}
		return result
	}
	function update() {
		// TOOD: use setTimeout to limit the rate of changes?
		if (localStorage) {
			var nns = RED.nodes.createCompleteNodeSet();
			localStorage.setItem(target(), JSON.stringify(nns));
			//console.log("localStorage write");
		}
	}
	function load() {
		if (localStorage) {
			var data = localStorage.getItem(target());
			//console.log("localStorage read: " + data);
			if (data) RED.nodes.import(data, false);
		}
	}
	function clear() {
		// TOOD: use setTimeout to limit the rate of changes?
		if (localStorage) {
			localStorage.removeItem(target());
			//console.log("localStorage write");
		}
	}
	return {
		update: update,
		load: load,
		clear: clear
	}
})();
