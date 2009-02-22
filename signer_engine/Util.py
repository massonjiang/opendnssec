#
# some generaly utility functions
#

import subprocess
import Util
import re
from datetime import timedelta

verbosity = 2;

def debug(level, message):
	if level <= verbosity:
		print(message)

def run_tool(command, input=None):
	Util.debug(5, "Command: '"+" ".join(command)+"'")
	if (input):
		p = subprocess.Popen(command, stdin=input, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
	else:
		p = subprocess.Popen(command, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
	return p

# months default to 31 days
# does not account for leap-seconds etc
# 3 ways to specify duration
DURATION_REGEX = re.compile("^P"
                            "(?:(?P<years>[0-9]+)Y)?"
                            "(?:(?P<months>[0-9]+)M)?"
                            "(?:(?P<weeks>[0-9]+)W)?"
                            "(?:(?P<days>[0-9]+)D)?"
                            "(?:T"
                            "(?:(?P<hours>[0-9]+)H)?"
                            "(?:(?P<minutes>[0-9]+)M)?"
                            "(?:(?P<seconds>[0-9]+)S)?"
                            ")?$"
                           )
DURATION_REGEX_ALT = re.compile("^P"
                                "(?P<years>[0-9]{4})"
                                "(?P<months>[0-9]{2})"
                                "(?P<weeks>)"
                                "(?P<days>[0-9]{2})"
                                "T"
                                "(?P<hours>[0-9]{2})"
                                "(?P<minutes>[0-9]{2})"
                                "(?P<seconds>[0-9]{2})"
                               )
DURATION_REGEX_ALT2 = re.compile("^P"
                                "(?P<years>[0-9]{4})"
                                "-(?P<months>[0-9]{2})"
                                "(?P<weeks>)"
                                "-(?P<days>[0-9]{2})"
                                "T"
                                "(?P<hours>[0-9]{2})"
                                ":(?P<minutes>[0-9]{2})"
                                ":(?P<seconds>[0-9]{2})"
                               )

def parse_duration(duration_string):
	match = DURATION_REGEX.match(duration_string)
	result = 0
	if not match:
		# raise error
		match = DURATION_REGEX_ALT.match(duration_string)
		if not match:
			match = DURATION_REGEX_ALT2.match(duration_string)
			if not match:
				raise Exception("Bad duration format: " +duration_string)

	g = match.group("years")
	if g:
		result += 31556926 * int(g)
	g = match.group("months")
	if g:
		result += 2678400 * int(g)
	g = match.group("weeks")
	if g:
		result += 604800 * int(g)
	g = match.group("days")
	if g:
		result += 86400 * int(g)
	g = match.group("hours")
	if g:
		result += 3600 * int(g)
	g = match.group("minutes")
	if g:
		result += 60 * int(g)
	g = match.group("seconds")
	if g:
		result += int(g)
	return result

