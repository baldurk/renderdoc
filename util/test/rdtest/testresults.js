document.body.onload = function() {
  function htmlEntityEncode(str) {
    return str.replace(/&/g, '&amp;')
              .replace(/"/g, '&quot;')
              .replace(/'/g, '&#x27;')
              .replace(/</g, '&lt;')
              .replace(/>/g, '&gt;')
  }

  function formatDiff(diff) {
    var difflines = diff.split('\n')

    if(difflines[0] == '')
      difflines.shift()

    strip = (/[^ \t]/.exec(difflines[0])).index

    for(var i=0; i < difflines.length; i++)
      difflines[i] = difflines[i].substr(strip)

    src = difflines.join('\n')

    return '<pre><code class="diff">' + htmlEntityEncode(src) + '</code></pre>';
  }

  // insert script tags to load highlight.js, so that starts immediately.
  {
    var script = document.createElement('script');
    script.setAttribute('src', 'https://cdnjs.cloudflare.com/ajax/libs/highlight.js/9.12.0/highlight.min.js');
    script.onload = function() {
      hljs.initHighlighting();
    };
    document.head.appendChild(script);

    css = document.createElement('link');
    css.setAttribute('rel', 'stylesheet');
    css.setAttribute('type', 'text/css');
    css.setAttribute('media', 'all');
    css.setAttribute('href', 'https://cdnjs.cloudflare.com/ajax/libs/highlight.js/9.12.0/styles/default.min.css');
    document.head.appendChild(css);

    css = document.createElement('link');
    css.setAttribute('rel', 'stylesheet');
    css.setAttribute('type', 'text/css');
    css.setAttribute('media', 'all');
    css.setAttribute('href', 'testresults.css');
    document.head.appendChild(css);
  }

  // First grab the source
  src = document.getElementById("logoutput").innerHTML;
  html = '';

  lines = src.split('\n');

  var test_name = '';
  var failed_tests = [];
  var indiff = false;
  var instack = false;
  var diff_text = '';
  var indent = 0;
  var commit = "v1.x";
  var basepath = "util/test/";
  var last_test = '';

  for(var i=0; i < lines.length; i++) {
    var line = lines[i].replace(/\t/g, '  ');
    var m = line.match(/^ *([.<>!=*#$+-\/]{2}) (.*)/);

    if(line.trim() == '')
      continue;

    if(m) {
      if(m[1] == '##') {
      	title = m[2].replace(/ ##$/, '');

        var hash = m[2].match(/Version ([0-9.]*) \(([a-f0-9]*)\)/);
        if(hash) {
        	document.title = title;
          commit = hash[2];
        }

        title = title.replace(commit, '<a href="https://github.com/baldurk/renderdoc/commit/' + commit + '">' + commit.substr(0, 8) + '</a>');

        html += '<h1>' + title + '</h1>';

      } else if(m[1] == '//') {
      	// comments, skip
      } else if(m[1] == '..') {
        html += '<div class="message">' + htmlEntityEncode(m[2]) + '</div>';
      } else if(m[1] == '!+') {
        html += '<div class="failure"><span class="message">' + htmlEntityEncode(m[2]) + '</span>';
      } else if(m[1] == '!-') {
        html += '</div>';
      } else if(m[1] == '!!') {
        html += '<div class="failure message">' + htmlEntityEncode(m[2]) + '</div>';
      } else if(m[1] == '**') {
        html += '<div class="success message">' + htmlEntityEncode(m[2]) + '</div>';
      } else if(m[1] == '==') {
        var comparison = m[2].match(/Compare: ([^ ]*)( \((.*)\))?/);

        var files = comparison[1].split(',');

        vs = '';
        for(var f=0; f < files.length; f++) {
          if(f > 0)
            vs += ' vs ';
          vs += `<a href="${files[f]}">${files[f]}</a>`;
        }

        if(comparison[3]) {
          diff = ` (<a href="${comparison[3]}">diff</a>)`;

          html += `<div class="expandable imgdiff"><span class="expandtoggle"></span><div class="title">${vs}${diff}</div><div class="contents">`;

          html += '<input type="range" min="0" max="100" value="50" class="img-slider" /> <label><input class="img-diff" type="checkbox">Show diff</label>';
          html += '<div class="img-comp-container">';
          html += ` <div class="img-comp img-a"><div class="img-clip"><img src="${files[0]}" /></div></div>`;
          html += ` <div class="img-comp img-b"><div class="img-clip"><img src="${files[1]}" /></div></div>`;
          html += ` <div class="img-comp img-diff hidden"><div class="img-clip"><img src="${comparison[3]}" /></div></div>`;
          html += '</div>';

          html += '</div>';
          html += '</div>';
        } else {
          html += `<div class="expandable imglist"><span class="expandtoggle"></span><div class="title">${vs}</div><div class="contents">`;
          for(var f=0; f < files.length; f++) {
            html += ` <img src="${files[f]}" />`;
          }
          html += '</div>';
          html += '</div>';
        }
      } else if(m[1] == '=+') {
        var comparison = m[2].match(/Compare: ([^ ]*)( \((.*)\))?/);

        var files = comparison[1].split(',');

        vs = '';
        for(var f=0; f < files.length; f++) {
          if(f > 0)
            vs += ' vs ';
          vs += `<a href="${files[f]}">${files[f]}</a>`;
        }

        html += `<div class="expandable diff"><span class="expandtoggle"></span><div class="title">${vs}</div><div class="contents">`;
        
        indiff = true;
        diff_text = '';
        indent += 4;
      } else if(m[1] == '=-') {
        html += '<pre><code class="diff">' + htmlEntityEncode(diff_text) + '</code></pre></div></div>';
        indiff = false;
        diff_text = '';
        indent -= 4;
      } else if(m[1] == '>>' || m[1] == '<<') {
        var start = (m[1] == '>>');
        var words = m[2].split(' ');

        indent += start ? 4 : -4;

        if(words[0] == 'Callstack') {
          html += start ? '<div class="expandable callstack"><span class="expandtoggle"></span><div class="title">Callstack</div><div class="contents"><pre>' : '</pre></div></div>';
          instack = start;
        } else if(words[0] == 'Test') {
          test_name = words[1];
          html += start ? '<div class="expandable test" id="' + test_name + '"><span class="expandtoggle"></span><div class="title">Test: ' + test_name + '</div><div class="contents">' : '</div></div>';

          if(start)
            last_test = test_name;
          else
            last_test = '';
        }
      } else if(m[1] == '$$') {
        if(m[2] == 'FAILED') {
          failed_tests.push(test_name)
        }
      }
    } else {
      if(indiff) {
        diff_text += line.substr(indent) + '\n';
      } else if(instack) {
        var frame = line.match(/File "(.*)", line (.*), in (.*)/);
        if(frame)
          html += `File <a href="https://github.com/baldurk/renderdoc/blob/${commit}/${basepath}${frame[1]}#L${frame[2]}">"${frame[1]}", line ${frame[2]}</a>, in ${frame[3]}\n`;
        else
          html += line.substr(indent) + '\n';
      } else {
        html += line.substr(indent) + '\n';
      }
    }
  }

  // now add on the html we generated so that it will render, then show it
  document.body.innerHTML += html;
  document.body.style.visibility = 'inherit';

  for(var i=0; i < failed_tests.length; i++) {
    var test = document.getElementById(failed_tests[i]);
    test.classList.add('expanded');
    test.classList.add('failed');
  }

  if(last_test != '') {
    var test = document.getElementById(last_test);
    test.classList.add('expanded');
  }

  var toggles = document.getElementsByClassName('expandtoggle');
  
  for(var i=0; i < toggles.length; i++) {
    toggles[i].addEventListener('click', function() {
      this.parentElement.classList.toggle('expanded');
    });
  }

  var sliders = document.getElementsByClassName('img-slider');
  
  for(var i=0; i < sliders.length; i++) {
    slidercallback = function(slider) {
      var img_comp_container = slider.nextElementSibling;
      while(img_comp_container && !img_comp_container.classList.contains("img-comp-container"))
        img_comp_container = img_comp_container.nextElementSibling;

      for(var c=0; c < img_comp_container.children.length; c++) {
        if(img_comp_container.children[c].classList.contains('img-b')) {
          img_comp_container.children[c].children[0].style.width = slider.value+'%';
          break;
        }
      }
    };
    sliders[i].addEventListener('input', function() { slidercallback(this); });
    slidercallback(sliders[i]);
  }

  var diffs = document.getElementsByClassName('img-diff');
  
  for(var i=0; i < diffs.length; i++) {
    diffs[i].addEventListener('change', function() {
      var img_comp_container = this.parentElement.nextElementSibling;
      while(img_comp_container && !img_comp_container.classList.contains("img-comp-container"))
        img_comp_container = img_comp_container.nextElementSibling;

      for(var c=0; c < img_comp_container.children.length; c++) {
        if(img_comp_container.children[c].classList.contains('img-diff')) {
          img_comp_container.children[c].classList.toggle('hidden');
          break;
        }
      }
    });
  }

  var imgcomps = document.getElementsByClassName('img-comp-container');
  
  for(var i=0; i < imgcomps.length; i++) {
    var img = imgcomps[i].children[1].children[0].children[0];
    (function(imgcomp)
    {
      img.addEventListener('load', function() {
        imgcomp.style.height = Math.max(300, img.height);
        var slider = imgcomp.previousElementSibling;
        while(slider && !slider.classList.contains("img-slider"))
          slider = slider.previousElementSibling;
        slider.style.width = Math.max(400, img.width);
      });
    }) (imgcomps[i]);
  }

};
