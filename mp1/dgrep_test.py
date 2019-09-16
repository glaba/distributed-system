#!/usr/bin/python
import random
import os
import subprocess
import sys

dictionary = ["I", "his", "that", "he", "was", "for", "on", "are", "with", "they", "be", "at", "one", "have", "this", "from", "by", "hot", "word", "but", "what", "some", "is", "it", "you", "or", "had", "the", "of", "to", "and", "a", "in", "we", "can", "out", "other", "were", "which", "do", "their", "time", "if", "will", "how", "said", "an", "each", "tell", "does", "set", "three", "want", "air", "well", "also", "play", "small", "end", "put", "home", "read", "hand", "port", "large", "spell", "add", "even", "land", "here", "must", "big", "high", "such", "follow", "act", "why", "ask", "men", "change", "went", "light", "kind", "off", "need", "house", "picture", "try", "us", "again", "animal", "point", "mother", "world", "near", "build", "self", "earth", "father", "any", "new", "work", "part", "take", "get", "place", "made", "live", "where", "after", "back", "little", "only", "round", "man", "year", "came", "show", "every", "good", "me", "give", "our", "under", "name", "very", "through", "just", "form", "sentence", "great", "think", "say", "help", "low", "line", "differ", "turn", "cause", "much", "mean", "before", "move", "right", "boy", "old", "too", "same", "she", "all", "there", "when", "up", "use", "your", "way", "about", "many", "then", "them", "write", "would", "like", "so", "these"]

input_valid = False
num_lines = 0
num_words_per_line = 0
word_to_grep = ""
port = 0
if len(sys.argv) == 5:
	try:
		num_lines = int(sys.argv[1])
		num_words_per_line = int(sys.argv[2])
		word_to_grep = str(sys.argv[3])
		port = int(sys.argv[4])
		if word_to_grep in dictionary:
			input_valid = True
		else:
			print("Provided word is not in dictionary")
	except ValueError:
		pass

if input_valid == False:
	print("dgrep_test.py <number of log lines> <number of words per line> <word to grep for> <port>")
	print("Dictionary of words used: " + ", ".join(dictionary))
	sys.exit()

reference = [0, 0, 0, 0, 0, 0, 0, 0, 0, 0]
for k in range(10):
	vm_name = "fa19-cs425-g60-" + str(k + 1).zfill(2) + ".cs.illinois.edu"

	try:
		os.system("ssh -o StrictHostKeyChecking=no -i ~/.ssh/id_rsa lawsonp2@" + vm_name + " \"rm /home/lawsonp2/machine.i.log\"")
		os.system("rm ~/tmp.log")
		os.system("touch ~/tmp.log")
	except OSError:
		pass # We don't care if the files didn't exist

	for i in range(num_lines):
		cur_line = ""
		has_word = False
		for j in range(num_words_per_line):
			word = random.choice(dictionary)
			if word == word_to_grep:
				has_word = True

			cur_line = cur_line + word + " "

		if has_word:
			reference[k] += 1

		os.system("echo " + cur_line + " >> ~/tmp.log")
	
	os.system("scp -o StrictHostKeyChecking=no -i ~/.ssh/id_rsa ~/tmp.log lawsonp2@" + vm_name + ":/home/lawsonp2/machine.i.log")

reference_str = ""
for k in range(10):
	reference_str += str(reference[k])
	if k != 9:
		reference_str += ", "

print("Reference word counts: " + reference_str)

os.system("dgrep_setup.sh " + str(port))
actual = [0, 0, 0, 0, 0, 0, 0, 0, 0, 0]
for k in range(10):
	vm_name = "fa19-cs425-g60-" + str(k + 1).zfill(2) + ".cs.illinois.edu"
	lines = subprocess.check_output("client " + vm_name + " " + str(port) + " " + word_to_grep, shell=True).split("\n")
	for j in range(len(lines)):
		if len(lines[j]) > 0:
			actual[k] += 1

actual_str = ""
for k in range(10):
	actual_str += str(actual[k])
	if k != 9:
		actual_str += ", "
print("DGrep word counts: " + actual_str)
