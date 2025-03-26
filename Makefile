test:
	docker run --rm -v ${PWD}/9cc:/9cc -w /9cc compilerbook make test

build:
	docker run --rm -v ${PWD}/9cc:/9cc -w /9cc compilerbook make build

sh:
	docker run --rm -it -v ${PWD}/9cc:/9cc -w /9cc compilerbook
