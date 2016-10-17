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

;(function (window, linkify, $) {
	var linkifyJquery = function (jquery, linkify) {
		'use strict';

		/**
  	Linkify a HTML DOM node
  */

		var tokenize = linkify.tokenize;
		var options = linkify.options;
		var Options = options.Options;


		var TEXT_TOKEN = linkify.parser.TOKENS.TEXT;

		var HTML_NODE = 1;
		var TXT_NODE = 3;
		/**
  	Given a parent element and child node that the parent contains, replaces
  	that child with the given array of new children
  */
		function replaceChildWithChildren(parent, oldChild, newChildren) {
			var lastNewChild = newChildren[newChildren.length - 1];
			parent.replaceChild(lastNewChild, oldChild);
			for (var i = newChildren.length - 2; i >= 0; i--) {
				parent.insertBefore(newChildren[i], lastNewChild);
				lastNewChild = newChildren[i];
			}
		}

		/**
  	Given an array of MultiTokens, return an array of Nodes that are either
  	(a) Plain Text nodes (node type 3)
  	(b) Anchor tag nodes (usually, unless tag name is overridden in the options)

  	Takes the same options as linkifyElement and an optional doc element
  	(this should be passed in by linkifyElement)
  */
		function tokensToNodes(tokens, opts, doc) {
			var result = [];

			for (var _iterator = tokens, _isArray = Array.isArray(_iterator), _i = 0, _iterator = _isArray ? _iterator : _iterator[Symbol.iterator]();;) {
				var _ref;

				if (_isArray) {
					if (_i >= _iterator.length) break;
					_ref = _iterator[_i++];
				} else {
					_i = _iterator.next();
					if (_i.done) break;
					_ref = _i.value;
				}

				var token = _ref;

				if (token.type === 'nl' && opts.nl2br) {
					result.push(doc.createElement('br'));
					continue;
				} else if (!token.isLink || !opts.check(token)) {
					result.push(doc.createTextNode(token.toString()));
					continue;
				}

				var _opts$resolve = opts.resolve(token);

				var formatted = _opts$resolve.formatted;
				var formattedHref = _opts$resolve.formattedHref;
				var tagName = _opts$resolve.tagName;
				var className = _opts$resolve.className;
				var target = _opts$resolve.target;
				var events = _opts$resolve.events;
				var attributes = _opts$resolve.attributes;

				// Build the link

				var link = doc.createElement(tagName);
				link.setAttribute('href', formattedHref);

				if (className) {
					link.setAttribute('class', className);
				}

				if (target) {
					link.setAttribute('target', target);
				}

				// Build up additional attributes
				if (attributes) {
					for (var attr in attributes) {
						link.setAttribute(attr, attributes[attr]);
					}
				}

				if (events) {
					for (var event in events) {
						if (link.addEventListener) {
							link.addEventListener(event, events[event]);
						} else if (link.attachEvent) {
							link.attachEvent('on' + event, events[event]);
						}
					}
				}

				link.appendChild(doc.createTextNode(formatted));
				result.push(link);
			}

			return result;
		}

		// Requires document.createElement
		function linkifyElementHelper(element, opts, doc) {

			// Can the element be linkified?
			if (!element || element.nodeType !== HTML_NODE) {
				throw new Error('Cannot linkify ' + element + ' - Invalid DOM Node type');
			}

			var ignoreTags = opts.ignoreTags;

			// Is this element already a link?
			if (element.tagName === 'A' || options.contains(ignoreTags, element.tagName)) {
				// No need to linkify
				return element;
			}

			var childElement = element.firstChild;

			while (childElement) {

				switch (childElement.nodeType) {
					case HTML_NODE:
						linkifyElementHelper(childElement, opts, doc);
						break;
					case TXT_NODE:

						var str = childElement.nodeValue;
						var tokens = tokenize(str);

						if (tokens.length === 0 || tokens.length === 1 && tokens[0] instanceof TEXT_TOKEN) {
							// No node replacement required
							break;
						}

						var nodes = tokensToNodes(tokens, opts, doc);

						// Swap out the current child for the set of nodes
						replaceChildWithChildren(element, childElement, nodes);

						// so that the correct sibling is selected next
						childElement = nodes[nodes.length - 1];

						break;
				}

				childElement = childElement.nextSibling;
			}

			return element;
		}

		function linkifyElement(element, opts) {
			var doc = arguments.length <= 2 || arguments[2] === undefined ? false : arguments[2];


			try {
				doc = doc || document || window && window.document || global && global.document;
			} catch (e) {/* do nothing for now */}

			if (!doc) {
				throw new Error('Cannot find document implementation. ' + 'If you are in a non-browser environment like Node.js, ' + 'pass the document implementation as the third argument to linkifyElement.');
			}

			opts = new Options(opts);
			return linkifyElementHelper(element, opts, doc);
		}

		// Maintain reference to the recursive helper to cache option-normalization
		linkifyElement.helper = linkifyElementHelper;
		linkifyElement.normalize = function (opts) {
			return new Options(opts);
		};

		// Applies the plugin to jQuery
		function apply($) {
			var doc = arguments.length <= 1 || arguments[1] === undefined ? false : arguments[1];


			$.fn = $.fn || {};

			try {
				doc = doc || document || window && window.document || global && global.document;
			} catch (e) {/* do nothing for now */}

			if (!doc) {
				throw new Error('Cannot find document implementation. ' + 'If you are in a non-browser environment like Node.js, ' + 'pass the document implementation as the second argument to linkify/jquery');
			}

			if (typeof $.fn.linkify === 'function') {
				// Already applied
				return;
			}

			function jqLinkify(opts) {
				opts = linkifyElement.normalize(opts);
				return this.each(function () {
					linkifyElement.helper(this, opts, doc);
				});
			}

			$.fn.linkify = jqLinkify;

			$(doc).ready(function () {
				$('[data-linkify]').each(function () {
					var $this = $(this);
					var data = $this.data();
					var target = data.linkify;
					var nl2br = data.linkifyNlbr;
					var options = {
						attributes: data.linkifyAttributes,
						defaultProtocol: data.linkifyDefaultProtocol,
						events: data.linkifyEvents,
						format: data.linkifyFormat,
						formatHref: data.linkifyFormatHref,
						nl2br: !!nl2br && nl2br !== 0 && nl2br !== 'false',
						tagName: data.linkifyTagname,
						target: data.linkifyTarget,
						className: data.linkifyClassName || data.linkifyLinkclass, // linkClass is deprecated
						validate: data.linkifyValidate,
						ignoreTags: data.linkifyIgnoreTags
					};
					var $target = target === 'this' ? $this : $this.find(target);
					$target.linkify(options);
				});
			});
		}

		// Try assigning linkifyElement to the browser scope
		try {
			var a = !define && (window.linkifyElement = linkifyElement);
		} catch (e) {}

		return apply;
	}($, linkify);
	if (typeof $.fn.linkify !== 'function') {
		linkifyJquery($);
	}
})(window, linkify, jQuery);
