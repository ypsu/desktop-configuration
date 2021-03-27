// Command basimark can convert between markdown and HTML representations.
// Does nothing if it detects that the input is the right format already.
package main

import (
	"bytes"
	"flag"
	"fmt"
	"io/ioutil"
	"log"
	"os"
	"regexp"
	"strings"
)

type mode int

const (
	noneMode mode = iota
	blockquoteMode
	pMode
	preMode
	ulMode
)

func close(output *bytes.Buffer, m mode) {
	switch m {
	case blockquoteMode:
		output.WriteString("</blockquote>")
	case pMode:
		output.WriteString("</p>")
	case preMode:
		output.WriteString("</pre>")
	case ulMode:
		output.WriteString("</li></ul>")
	}
}

// autolinks is something like "doc|go|groups|oncall|sheets|who".
// Those will be autolinkified if they are followed by a slash.
// Leave it empty if no autolinkification is needed.
func toHTML(inputbuf []byte, autolinks []byte) []byte {
	output := &bytes.Buffer{}

	// Escape HTML characters.
	input := strings.NewReplacer("&", "&amp;", "<", "&lt;", ">", "&gt;", "\"", "&quot;", "'", "&#039;").Replace(string(inputbuf))

	// Linkify links.
	if len(autolinks) == 0 {
		autolinks = []byte("__autolink_placeholder__")
	}
	re := regexp.MustCompile("\\b((http(s)?://([-.a-z0-9]+)/?)|(" + string(autolinks) + ")/)(\\S*)?\\b")
	input = re.ReplaceAllString(input, "<a href='http$3://$4$5/$6'>$0</a>")

	// Add a some styling.
	output.WriteString("<div style=max-width:50em>")

	// HTMLize the input markdown.
	m := noneMode
	for ln, line := range strings.Split(input, "\n") {
		if len(line) == 0 {
			close(output, m)
			m = noneMode
		} else if line[0] == ' ' {
			if m == noneMode {
				m = preMode
				if len(strings.TrimSpace(line)) == 0 {
					line += "<pre>"
				} else {
					output.WriteString("<pre>")
				}
			}
		} else if line[0] == '-' {
			if m == noneMode {
				m = ulMode
				output.WriteString("<ul><li>")
				line = "<span style=display:none>-</span>" + line[1:]
			} else if m == ulMode {
				output.WriteString("</li><li>")
				line = "<span style=display:none>-</span>" + line[1:]
			}
		} else if strings.HasPrefix(line, "# ") {
			if m != noneMode {
				log.Fatalf("line %d: # must be starting its own paragraph: %s", ln+1, line)
			}
			line = "<p style=font-weight:bold>" + line + "</p>"
		} else if strings.HasPrefix(line, "&gt;") {
			if m == noneMode {
				m = blockquoteMode
				output.WriteString("<blockquote style='border-left:solid 1px;padding:0 0.5em'>")
			}
			if m == blockquoteMode {
				line = "<span style=display:none>&gt;</span>" + line[4:]
			}
		} else {
			if m == noneMode {
				m = pMode
				output.WriteString("<p>")
			}
		}
		output.WriteString(line)
		output.WriteByte('\n')
	}
	close(output, m)
	return append(bytes.TrimRight(output.Bytes(), "\n"), []byte("</div>\n")...)
}

func toMarkdown(inputbuf []byte) []byte {
	// Remove HTML tags.
	re := regexp.MustCompile("<[^>]*>")
	outputbuf := re.ReplaceAll(inputbuf, []byte(""))

	// Remove spurious newlines.
	re = regexp.MustCompile("\n\n\n+")
	outputbuf = re.ReplaceAll(append(bytes.TrimSpace(outputbuf), '\n'), []byte("\n\n"))

	// Restore escaped characters.
	output := strings.NewReplacer("&lt;", "<", "&gt;", ">", "&quot;", "\"", "&#039;", "'").Replace(string(outputbuf))
	output = strings.ReplaceAll(output, "&amp;", "&")

	return []byte(output)
}

func main() {
	// Set up flags.
	rFlag := flag.Bool("r", false, "Restore the HTML back to the original text.")
	flag.Usage = func() {
		fmt.Fprintln(os.Stderr, "Usage: basimark <input >output")
		fmt.Fprintln(os.Stderr, "input is markdown, output is html unless reversed with the -r flag.")
		fmt.Fprintln(os.Stderr, "Flags:")
		flag.PrintDefaults()
	}
	flag.Parse()

	// Read input buffers.
	inputbuf, err := ioutil.ReadAll(os.Stdin)
	if err != nil {
		log.Fatal(err)
	}

	// Read autolinks if available.
	autolinks, _ := ioutil.ReadFile(os.Getenv("HOME") + "/.autolinks")
	autolinks = bytes.TrimSpace(autolinks)

	// Run the conversion.
	outbuf := inputbuf
	isHTML := bytes.HasPrefix(inputbuf, []byte("<"))
	if *rFlag && isHTML {
		outbuf = toMarkdown(inputbuf)
	} else if !*rFlag && !isHTML {
		outbuf = toHTML(inputbuf, autolinks)
	}
	ioutil.WriteFile("/dev/stdout", outbuf, 0)
}
