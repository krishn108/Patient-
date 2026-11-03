from flask import Flask, render_template, request, redirect, url_for, flash
import ctypes
import os

app = Flask(__name__)
app.secret_key = "change-this-demo-secret"

BASE_DIR = os.path.dirname(os.path.abspath(__file__))
DATA_DIR = os.path.join(BASE_DIR, "data")

os.makedirs(DATA_DIR, exist_ok=True)

LIBRARY_PATH = os.path.join(BASE_DIR, "libpatient.so")
c_backend = ctypes.CDLL(LIBRARY_PATH)

# C function declarations
c_backend.patient_init.restype = None

c_backend.patient_add.argtypes = [
    ctypes.c_char_p, ctypes.c_char_p,
    ctypes.c_char_p, ctypes.c_char_p
]
c_backend.patient_add.restype = None

c_backend.donor_add.argtypes = [
    ctypes.c_char_p, ctypes.c_char_p, ctypes.c_char_p
]
c_backend.donor_add.restype = None

c_backend.stock_add.argtypes = [ctypes.c_char_p, ctypes.c_int]
c_backend.stock_add.restype = ctypes.c_int

c_backend.issue_blood.argtypes = [
    ctypes.c_char_p, ctypes.c_char_p, ctypes.c_int
]
c_backend.issue_blood.restype = ctypes.c_int

c_backend.request_add.argtypes = [
    ctypes.c_char_p, ctypes.c_char_p, ctypes.c_int
]
c_backend.request_add.restype = None

c_backend.request_process.restype = ctypes.c_int

for function_name in [
    "patient_list", "donor_list", "stock_list",
    "request_list", "request_history"
]:
    function = getattr(c_backend, function_name)
    function.argtypes = [ctypes.c_char_p, ctypes.c_int]
    function.restype = None


def ctext(value):
    return value.strip().replace("|", " ").replace("\n", " ").encode("utf-8")


def get_c_data(function):
    output = ctypes.create_string_buffer(20000)
    function(output, len(output))
    return output.value.decode("utf-8", errors="replace")


def valid_blood(value):
    return value in ["A+", "A-", "B+", "B-", "AB+", "AB-", "O+", "O-"]


# Create the data files if they do not exist
c_backend.patient_init()


@app.route("/")
def index():
    return render_template(
        "index.html",
        patients=get_c_data(c_backend.patient_list),
        donors=get_c_data(c_backend.donor_list),
        stock=get_c_data(c_backend.stock_list),
        requests_data=get_c_data(c_backend.request_list),
        issues=get_c_data(c_backend.request_history)
    )


@app.route("/add_patient", methods=["POST"])
def add_patient():
    name = ctext(request.form.get("name", ""))
    age_text = request.form.get("age", "")
    blood = request.form.get("blood", "")
    severity = request.form.get("severity", "")

    try:
        age = int(age_text)
        level = int(severity)
    except ValueError:
        flash("Please enter a valid age and severity level.")
        return redirect(url_for("index"))

    if not name or age < 1 or age > 120 or not valid_blood(blood) or level not in range(1, 6):
        flash("Invalid patient details.")
        return redirect(url_for("index"))

    c_backend.patient_add(
        name, str(age).encode("utf-8"),
        blood.encode("utf-8"), str(level).encode("utf-8")
    )

    flash("Patient registered.")
    return redirect(url_for("index"))


@app.route("/add_donor", methods=["POST"])
def add_donor():
    name = ctext(request.form.get("donor_name", ""))
    age_text = request.form.get("donor_age", "")
    blood = request.form.get("donor_blood", "")

    try:
        age = int(age_text)
    except ValueError:
        flash("Please enter a valid donor age.")
        return redirect(url_for("index"))

    if not name or age < 18 or age > 100 or not valid_blood(blood):
        flash("Invalid donor details.")
        return redirect(url_for("index"))

    c_backend.donor_add(
        name, str(age).encode("utf-8"), blood.encode("utf-8")
    )

    flash("Donor registered.")
    return redirect(url_for("index"))


@app.route("/add_stock", methods=["POST"])
def add_stock():
    blood = request.form.get("stock_blood", "")
    try:
        units = int(request.form.get("units", "0"))
    except ValueError:
        units = 0

    if not valid_blood(blood) or units <= 0:
        flash("Enter a valid blood group and positive units.")
    elif c_backend.stock_add(blood.encode("utf-8"), units):
        flash("Blood stock updated.")
    else:
        flash("Could not update blood stock.")

    return redirect(url_for("index"))


@app.route("/add_request", methods=["POST"])
def add_request():
    patient = ctext(request.form.get("request_patient", ""))
    blood = request.form.get("request_blood", "")

    try:
        units = int(request.form.get("request_units", "0"))
    except ValueError:
        units = 0

    if not patient or not valid_blood(blood) or units <= 0:
        flash("Enter valid blood request details.")
    else:
        c_backend.request_add(
            patient, blood.encode("utf-8"), units
        )
        flash("Blood request added to the pending queue.")

    return redirect(url_for("index"))


@app.route("/issue_blood", methods=["POST"])
def issue_blood():
    patient = ctext(request.form.get("issue_patient", ""))
    blood = request.form.get("issue_blood_group", "")

    try:
        units = int(request.form.get("issue_units", "0"))
    except ValueError:
        units = 0

    if not patient or not valid_blood(blood) or units <= 0:
        flash("Enter valid blood issue details.")
    else:
        result = c_backend.issue_blood(
            patient, blood.encode("utf-8"), units
        )

        if result == 1:
            flash("Blood issued successfully. Stock deducted.")
        else:
            flash("Issue failed. Check available stock.")

    return redirect(url_for("index"))


@app.route("/process_request", methods=["POST"])
def process_request():
    result = c_backend.request_process()

    if result == 1:
        flash("Next request processed. Stock deducted and issue recorded.")
    elif result == 2:
        flash("Next request could not be fulfilled due to insufficient stock.")
    else:
        flash("No pending request found, or request processing failed.")

    return redirect(url_for("index"))


if __name__ == "__main__":
    app.run(debug=True)
