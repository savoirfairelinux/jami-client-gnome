/*
 * Copyright (c) 2016 SoapBox Innovations Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
*/

'use strict';

;(function (window, linkify) {
	var linkifyString = function (linkify) {
		'use strict';

		/**
  	Convert strings of text into linkable HTML text
  */

		var tokenize = linkify.tokenize;
		var options = linkify.options;
		var Options = options.Options;


		function escapeText(text) {
			return text.replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;');
		}

		function escapeAttr(href) {
			return href.replace(/"/g, '&quot;');
		}

		function attributesToString(attributes) {
			if (!attributes) {
				return '';
			}
			var result = [];

			for (var attr in attributes) {
				var val = attributes[attr] + '';
				result.push(attr + '="' + escapeAttr(val) + '"');
			}
			return result.join(' ');
		}

		function linkifyStr(str) {
			var opts = arguments.length <= 1 || arguments[1] === undefined ? {} : arguments[1];

			opts = new Options(opts);

			var tokens = tokenize(str);
			var result = [];

			for (var i = 0; i < tokens.length; i++) {
				var token = tokens[i];

				if (token.type === 'nl' && opts.nl2br) {
					result.push('<br>\n');
					continue;
				} else if (!token.isLink || !opts.check(token)) {
					result.push(escapeText(token.toString()));
					continue;
				}

				var _opts$resolve = opts.resolve(token);

				var formatted = _opts$resolve.formatted;
				var formattedHref = _opts$resolve.formattedHref;
				var tagName = _opts$resolve.tagName;
				var className = _opts$resolve.className;
				var target = _opts$resolve.target;
				var attributes = _opts$resolve.attributes;


				var link = '<' + tagName + ' href="' + escapeAttr(formattedHref) + '"';

				if (className) {
					link += ' class="' + escapeAttr(className) + '"';
				}

				if (target) {
					link += ' target="' + escapeAttr(target) + '"';
				}

				if (attributes) {
					link += ' ' + attributesToString(attributes);
				}

				link += '>' + escapeText(formatted) + '</' + tagName + '>';
				result.push(link);
			}

			return result.join('');
		}

		if (!String.prototype.linkify) {
			String.prototype.linkify = function (opts) {
				return linkifyStr(this, opts);
			};
		}

		return linkifyStr;
	}(linkify);
	window.linkifyStr = linkifyString;
})(window, linkify);
