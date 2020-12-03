// Copyright 2018 Global Phasing Ltd.

#define GEMMI_WRITE_IMPLEMENTATION
#include "gemmi/sprintf.hpp"
#include "gemmi/to_mmcif.hpp"
#include "gemmi/to_pdb.hpp"
#include "gemmi/mtz.hpp"
#include "gemmi/fstream.hpp"

#include "common.h"

namespace py = pybind11;
using namespace gemmi;

void add_write(py::module& m, py::class_<Structure>& structure) {
  structure
    .def("make_pdb_headers", &make_pdb_headers)
    .def("write_pdb", [](const Structure& st, const std::string& path,
                         bool seqres_records, bool ssbond_records,
                         bool link_records, bool cispep_records,
                         bool ter_records, bool numbered_ter,
                         bool ter_ignores_type, bool use_linkr) {
       PdbWriteOptions options;
       options.seqres_records = seqres_records;
       options.ssbond_records = ssbond_records;
       options.link_records = link_records;
       options.cispep_records = cispep_records;
       options.ter_records = ter_records;
       options.numbered_ter = numbered_ter;
       options.ter_ignores_type = ter_ignores_type;
       options.use_linkr = use_linkr;
       Ofstream f(path);
       write_pdb(st, f.ref(), options);
    }, py::arg("path"),
       py::arg("seqres_records")=true, py::arg("ssbond_records")=true,
       py::arg("link_records")=true, py::arg("cispep_records")=true,
       py::arg("ter_records")=true, py::arg("numbered_ter")=true,
       py::arg("ter_ignores_type")=false, py::arg("use_linkr")=false)
    .def("write_minimal_pdb",
         [](const Structure& st, const std::string& path) {
       Ofstream f(path);
       write_minimal_pdb(st, f.ref());
    }, py::arg("path"))
    .def("make_minimal_pdb", [](const Structure& st) -> std::string {
       std::ostringstream os;
       write_minimal_pdb(st, os);
       return os.str();
    })
    .def("make_mmcif_document", &make_mmcif_document)
    .def("make_mmcif_headers", &make_mmcif_headers)
    //.def("update_mmcif_block", &update_mmcif_block)
    ;
}
