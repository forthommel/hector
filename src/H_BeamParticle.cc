  /* * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *                                                         *
*                   --<--<--  A fast simulator --<--<--     *
*                 / --<--<--     of particle   --<--<--     *
*  ----HECTOR----<                                          *
*                 \ -->-->-- transport through -->-->--     *
*                   -->-->-- generic beamlines -->-->--     *
*                                                           *
* JINST 2:P09005 (2007)                                     *
*      X Rouby, J de Favereau, K Piotrzkowski (CP3)         *
*       http://www.fynu.ucl.ac.be/hector.html               *
*                                                           *
* Center for Cosmology, Particle Physics and Phenomenology  *
*              Universite catholique de Louvain             *
*                 Louvain-la-Neuve, Belgium                 *
 *                                                         *
   * * * * * * * * * * * * * * * * * * * * * * * * * * * */

/// \file H_BeamParticle.cc
/// \brief Class aiming at simulating a particle in the LHC beam

// from IP to RP, with emission of a photon of defined energy and Q.
// Units : angles [rad], distances [m], energies [GeV], c=[1].
// !!! no comment statement at the end of a #define line !!!

// c++ #includes
#include <iostream>
#include <iomanip>
#include <vector>
#include <string>
#include <cmath>

// ROOT #includes
#include "H_Parameters.h"
#include "TRandom.h"
#include "TVectorD.h"
#ifdef _include_pythia_
#include "TPythia6.h"
#endif

// local #includes
#include "H_OpticalElement.h"
#include "H_BeamParticle.h"
#include "H_Drift.h"

using namespace std;

void H_BeamParticle::init() { // for more efficiency, put the objects away from init!
	mp = MP;
	qp = QP;
        fx = 0;
        fy = 0;
        thx = 0;
        thy = 0;
        fs = 0;
        //energy = BE_DEF; //FIXME
        hasstopped = false;
        hasemitted = false;
	isphysical = true;
	addPosition(fx,thx,fy,thy,fs);
	stop_position = new TVectorD(LENGTH_VEC);
	for (int i=0; i<LENGTH_VEC; i++) (*stop_position)[i] = -1;
	stop_element = 0;
}

H_BeamParticle::H_BeamParticle() { 
	init();
}

H_BeamParticle::H_BeamParticle(const H_BeamParticle& p): 
	mp(p.mp), qp(p.qp), fs(p.fs), fx(p.fx), fy(p.fy), thx(p.thx), thy(p.thy), 
	energy(p.energy), hasstopped(p.hasstopped), hasemitted(p.hasemitted),
	isphysical(p.isphysical), stop_position(new TVectorD(*(p.stop_position))),
	stop_element(0), positions(p.positions) {
	if(p.hasstopped) stop_element = new H_OpticalElement(*(p.stop_element)); 
}

H_BeamParticle::H_BeamParticle(const double mass, const double charge) {
	init();
	mp = mass;
	qp = (mass==0) ? 0 : charge;
	/// rejects particles with mass = 0 and charge != 0
}

H_BeamParticle& H_BeamParticle::operator=(const H_BeamParticle& p) {
	if(this==&p) return *this;
	mp = p.mp;
	qp = p.qp;
        fx = p.fx;
        fy = p.fy;
        thx = p.thx;
        thy = p.thy;
        fs = p.fs;
        energy = p.energy;
        hasstopped = p.hasstopped;
        hasemitted = p.hasemitted;
	isphysical = p.isphysical;
        stop_position = new TVectorD(*(p.stop_position));
	if(p.hasstopped) stop_element = new H_OpticalElement(*(p.stop_element));
	else stop_element = 0;
        positions = p.positions;
	return *this;
}

const bool H_BeamParticle::stopped(const H_AbstractBeamLine * beamline) {
	vector<TVectorD>::const_iterator position_i;
	for(position_i = positions.begin(); position_i < positions.end()-1; position_i++) {
		const unsigned int pos = position_i-positions.begin();
		if(beamline->getElement(pos)->getAperture()->getType()!=NONE) {
			bool has_passed_entrance = beamline->getElement(pos)->isInside((*position_i)[INDEX_X],(*position_i)[INDEX_Y]);
			bool has_passed_exit     = beamline->getElement(pos)->isInside((*(position_i+1))[INDEX_X],(*(position_i+1))[INDEX_Y]);
			// here we should distinguish between particles passing the input or not.
			//  - particles not passing the input are logically stopped at the input position. period.
			//  - particles passing the input but not the output are stopped somewhere in the element
			//  - particles passing both the input and the output could have been stopped somewhere inside too (less likely)
			if(!has_passed_entrance) {
				if(VERBOSE) cout<<"Stopped at the entrance of "<<beamline->getElement(pos)->getName();
				hasstopped=true;
				stop_element = const_cast<H_OpticalElement*>(beamline->getElement(pos));
				*stop_position = *position_i;
				if(VERBOSE) cout<<" at s = "<<(*stop_position)[4]<<endl;
				return hasstopped;
			} else if (!has_passed_exit) {
				if(VERBOSE) cout<<"Stopped inside "<<beamline->getElement(pos)->getName();
				hasstopped=true;
				stop_element = const_cast<H_OpticalElement*>(beamline->getElement(pos));
				// this should be computed using the element-based method "H_OpticalElement::getHitPosition" (caution : always nonlinear).
				*stop_position = beamline->getElement(pos)->getHitPosition(*position_i,beamline->getBeamEnergy()-energy,mp,qp);
				if(VERBOSE) cout<<" at s = "<<(*stop_position)[4]<<endl;
				return hasstopped;
			}
/*
			if(!(has_passed_entrance && has_passed_exit)) {
				if(VERBOSE) cout<<"particle stopped at "<<(beamline->getElement(pos))->getName();
				if(VERBOSE) cout<<" (s = "<<(*position_i)[4] << ")" << endl;
				hasstopped=true;
				stop_element = const_cast<H_OpticalElement*>(beamline->getElement(pos));
				*stop_position = *position_i;
				return hasstopped;
			} // if
*/
		} // if
	} // for
	return hasstopped;
}

void H_BeamParticle::addPosition(const double x, const double tx, const double y, const double ty, const double s) {
			// [x] = [y] = m ; [tx] = [ty] = rad; [s] = m
		double xys[LENGTH_VEC];
		xys[INDEX_X]=x;
		xys[INDEX_TX]=tx;
		xys[INDEX_Y]=y;
		xys[INDEX_TY]=ty;
		xys[INDEX_S]=s;
		
		TVectorD temp_vec(LENGTH_VEC,xys);
		positions.push_back(temp_vec);
}



void H_BeamParticle::smearPos(const double dx,const double dy, TRandom* r) {
  // the beam is centered on (fx,fy) at IP
        fx = r->Gaus(fx,dx);
        fy = r->Gaus(fy,dy);
        positions.clear();
        addPosition(fx,thx,fy,thy,fs);
        return;
}

void H_BeamParticle::smearAng(const double tx, const double ty, TRandom* r) {
  // the beam transverse direction is centered on (thx,thy) at IP
        thx = r->Gaus(thx,tx);
        thy = r->Gaus(thy,ty);
	positions.clear();
	addPosition(fx,thx,fy,thy,fs);
	return;
}

void H_BeamParticle::smearE(const double erre, TRandom* r) {
	energy = r->Gaus(energy,erre);
	return;
}

void H_BeamParticle::smearS(const double errs, TRandom* r) {
        fs= r->Gaus(fs,errs);
	positions.clear();
	addPosition(fx,thx,fy,thy,fs);
        return;
}


void H_BeamParticle::set4Momentum(const double px, const double py, const double pz, const double ene) {
	/// @param px, py, pz, ene is \f$ (\vec p , E) [GeV]\f$ 
	///
	/// Clears the H_BeamParticle::positions vector.
	positions.clear();
	if(pz==0) {
		cout<<" ERROR in H_BeamParticle::set4Momentum : no momentum in the beamline direction !"<<endl;
		return;
	}
        thx = thx + URAD*atan(px/pz);
        thy = thy + URAD*atan(py/pz);
        energy = ene;
	positions.clear();
        addPosition(fx,thx,fy,thy,fs);
	return;
}

void H_BeamParticle::set4Momentum(const TLorentzVector& pmu) {
	/// @param pmu is the particle 4-momentum \f$ p^\mu \f$
	///
	/// Clears the H_BeamParticle::positions vector.
	set4Momentum(pmu.Px(), pmu.Py(), pmu.Pz(), pmu.E());
}

void H_BeamParticle::setE(const double ene) {
	energy = ene;
	return;
}

void H_BeamParticle::setPosition(const double x, const double y, const double tx, const double ty, const double s) {
	/// @param x, y are the transverse positions in \f$ \mu \f$ m
	/// @param tx, ty are the angles in \f$ \mu \f$ rad
	/// @param s is the longitudinal coordinate in m
	// clear positions and sets the initial one.
		fx=x;
		fy=y;
		thx=tx;
		thy=ty;
		fs = s;
		positions.clear();
		addPosition(fx,thx,fy,thy,s);

	return;
}

const H_OpticalElement* H_BeamParticle::getStoppingElement() const{
	if(hasstopped) return stop_element;
	else { H_OpticalElement * dummy_el = new H_Drift("",0,0,0); return dummy_el;}
}

void H_BeamParticle::emitGamma(const double gee, const double gq2, const double phimin, const double phimax) {
	/// @param gee = \f$ E_{\gamma} \f$ is the photon energy
	/// @param gq2 = Q < 0 is virtuality of photon \f$ Q^{2} = E^{2}-\vec{k}^{2} \f$
	/// @param phimin : lower bound for \f$ phi \f$
	/// @param phimax : higher bound for \f$ phi \f$

	if(gq2==0) { 
		if(VERBOSE) cout<<"No virtuality : only energy has changed"<<endl;
		setE(energy-gee);
	    return;	
	}

	double m1 = mp;

	double e1 = energy , e2 = energy - gee; // particle energy : before (1) / after (2)

	double p1 = sqrt(pow(e1,2) - pow(m1,2)), p2 = sqrt(pow(e2,2) - pow(m1,2)); // particle momentum : before (1) / after (2)
	double q2min = pow(gee,2) - pow(p1+p2,2); // lower bound from kinematics E^2 - (p1 + p2)^2
	double q2max = -2 * pow(m1*gee/(p1+p2),2) * (1 + (pow(e1,2) + pow(e2,2) -pow(m1,2))/(e1*e2 + p1*p2) );
			// upper bound from kinematics ; E^2 - (p1-p2)^2; is bad for numerical computations


	// if q2min < q2 < q2max is NOT true, there will be mathematical problems (like cos(eta) > 1).
	// so if q2 > q2max, we force q2 = q2max (-> cos(eta) = 1)
	// and if q2 < q2min, we force q2 = q2min (-> cos(eta) = 1)
	// BUT the user knows something was wrong with the value of "H_BeamParticle::isphysical"
	const double q2 = (gq2 > q2max ) ? q2max : (gq2 < q2min) ? q2min : gq2;
	
	if( (gq2>q2max) || (gq2<q2min)) {
		if(VERBOSE) cout<<"<H_BeamParticle> WARNING : Non physical particle ! Q2 (" << q2 << ") and E ("<<gee << ") are not compatible." << endl; 
		isphysical = false; 
	}

	if(hasemitted) { cout<<"<H_BeamParticle> WARNING : particle has already emitted at least one gamma !"<<endl;}
	hasemitted = true;
	energy = energy - gee;
	// gkk is k
	double gkk = sqrt(pow(gee,2)-q2);
	// eta is the angle between gamma and initial direction of the gamma-emitting particle
        // ceta = cos(eta) and seta = sin(eta)

	double ceta = sqrt( pow(mp/p1,2) + 1  ) * sqrt( q2/pow(gkk,2) + 1 ) - q2/(2*p1*gkk);
	double seta = sqrt(1 - ceta*ceta); 
	// theta is the angle between particle and beam
        double theta = URAD*atan(seta/(BE_DEF/gkk - ceta));
        double phi = phimin + gRandom->Uniform(phimax-phimin);
        thx = thx + theta*cos(phi);
        thy = thy - theta*sin(phi);

	// caution : emitting a photon erases all known positions !
	positions.clear();
	addPosition(fx,thx,fy,thy,fs);
	return;
}

void H_BeamParticle::doInelastic() {
#ifdef _include_pythia_
//	if(!gROOT->GetClass("TPythia6")) {
//		gROOT->Reset();
//	        gSystem->Load("libPythia6");
//	        gSystem->Load("libEG");
//	        gSystem->Load("libEGPythia6");
//	}
	
	TPythia6 gen;
	// select AB -> AX process 
        gen.SetMSEL(0);
        gen.SetMSUB(93,1);
	// no showers/decays
	gen.SetMSTP(111,0);
	// no printouts
	gen.SetMSTP(122,0);
	gen.SetMSTU(12,0);
	// generator initialization
        gen.Initialize("CMS","p","p",14000);
	// event generation
	gen.GenerateEvent();
	// list particles
//	gen.Pylist(1);
	thx = thx + URAD*atan(gen.GetP(5,1)/gen.GetP(5,3));
	thy = thy + URAD*atan(gen.GetP(5,2)/gen.GetP(5,3));
	energy = gen.GetP(5,4);
	positions.clear();
	addPosition(fx,thx,fy,thy,fs);
#endif
	return;
}

std::ostream& operator<< (std::ostream& os, const H_BeamParticle& p) {
        os << " M   = " << p.getM()  << "GeV ";
        os << " Q   = " << p.getQ()  << "e";
        os << " fx  = " << p.getX()  << "m   ";
        os << " fy  = " << p.getY()  << "m   ";
        os << " thx = " << p.getTX() << "rad ";
        os << " thy = " << p.getTY() << "rad ";
        os << endl;
   return os;
}

/// The phase space vector is (x,x',y,y',E)
/// [x] = [y] = meters
/// [x'] = [y'] = 1  with x' = dx/ds = tan (thetaX)
/// [E] = GeV
const TMatrixD * H_BeamParticle::getV() const {
	double vec[MDIM] = {fx/URAD, tan(thx/URAD), fy/URAD, tan(thy/URAD),energy};
	TMatrixD * mat = new TMatrixD(1,MDIM,vec);
	return mat;
}	

void H_BeamParticle::printV() const {
	TMatrixD X(*getV());
	cout << " x  = " << (X.GetMatrixArray())[0] << "m ";
	cout << " x' = " << (X.GetMatrixArray())[1] << "  ";
	cout << " y  = " << (X.GetMatrixArray())[2] << "m ";
	cout << " y' = " << (X.GetMatrixArray())[3] << "  ";
	cout << endl;
	return;
}

void H_BeamParticle::propagate(const double position) {
	/// @param position is the s coordinate in m to reach
	/// Does not propagate if position is in the middle of an otics element of the beamline.
        vector<TVectorD>::const_iterator position_i = positions.begin();
	double l = 0.;
	if(position != fs) { // avoid repeating the computation if already done at this position
		if(position == (*position_i)[INDEX_S]) {
			fs = position;
			fx = (*position_i)[INDEX_X];
			fy = (*position_i)[INDEX_Y];
			thx= (*position_i)[INDEX_TX];
			thy= (*position_i)[INDEX_TY]; 
			return;
		} else 
        	for(position_i = positions.begin(); position_i < positions.end(); position_i++) {
			if((*position_i)[INDEX_S]>=position) {
				if(position_i==positions.begin()) {
					if(VERBOSE) cout<<"<H_BeamParticle> ERROR : non reachable value"<<endl;
					return;
				}
				l = (*position_i)[INDEX_S] - (*(position_i-1))[INDEX_S];
				if(l==0) {
					if(VERBOSE) cout<<"<H_BeamParticle> WARNING : no luck in choosing position, no propagation done"<<endl;
					return;
				}
				fs = position;
				fx = (*(position_i-1))[INDEX_X] + (position-(*(position_i-1))[INDEX_S])*((*position_i)[INDEX_X] - (*(position_i-1))[INDEX_X])/l;
				fy = (*(position_i-1))[INDEX_Y] + (position-(*(position_i-1))[INDEX_S])*((*position_i)[INDEX_Y] - (*(position_i-1))[INDEX_Y])/l;
				thx = (*(position_i-1))[INDEX_TX];
				thy = (*(position_i-1))[INDEX_TY];
				return;
			}
		}
		position_i = positions.begin();
		cout << "Desired position is : " << position << " & positions.begin() is " << (*position_i)[INDEX_S] << endl;
		cout<<"<H_BeamParticle> ERROR : position not reachable"<<endl;	
		return;
	}
}

/// Caution : do not use this method (obsolete) !!!
void H_BeamParticle::propagate(const H_AbstractBeamLine * beam, const H_OpticalElement * element) {
	TMatrixD X(*getV());
	X *= beam->getPartialMatrix(element);
	fx = URAD*(X.GetMatrixArray())[0];
	thx = URAD*atan((X.GetMatrixArray())[1]);
	fy = URAD*(X.GetMatrixArray())[2];
	thy = URAD*atan((X.GetMatrixArray())[3]);
	return;
}

void H_BeamParticle::propagate(const H_AbstractBeamLine * beam, const string& el_name) {
	propagate(beam->getElement(el_name)->getS()+beam->getElement(el_name)->getLength());
	return;
}

void H_BeamParticle::propagate(const H_AbstractBeamLine * beam) {
	TMatrixD X(*getV());
	X  *= beam->getBeamMatrix();
	fx  = URAD*(X.GetMatrixArray())[0];
	thx = URAD*atan((X.GetMatrixArray())[1]);
	fy  = URAD*(X.GetMatrixArray())[2];
	thy = URAD*atan((X.GetMatrixArray())[3]);
	return;
}

void H_BeamParticle::showPositions() const{
	vector<TVectorD>::const_iterator position_i;
	TVectorD temp_vec(LENGTH_VEC);

	for(position_i = positions.begin(); position_i < positions.end(); position_i++) {
		cout << "Vector (x,y,s) = (" << (*position_i)[INDEX_X] << ", " << (*position_i)[INDEX_Y] << ", " << (*position_i)[INDEX_S] << "). " << endl;
	}
	return ;
}

TGraph * H_BeamParticle::getPath(const int x_or_y, const int color) const{
        /// @param x_or_y = 0(1) draws the x(y) component;

        const int N = (int) positions.size();
		int mycolor = color;
        if(N<2) cout<<"<H_BeamParticle> WARNING : particle positions not calculated : please run computePath"<<endl; 
        double * s = new double[N], * graph = new double[N];

        int index;
        if(x_or_y==0) {index = INDEX_X;} else {index = INDEX_Y;}

        vector<TVectorD>::const_iterator position_i;
        for(position_i = positions.begin(); position_i < positions.end(); position_i++) {
                graph[(int)(position_i-positions.begin())] = (*position_i)[index];
                s[(int)(position_i-positions.begin())] = (*position_i)[INDEX_S];
        }

        TGraph * ppath = new TGraph(N,s,graph);
        ppath->SetLineColor(mycolor);
	delete [] s;
	delete [] graph;
        return ppath;
}

// should be removed later, to keep only computePath(const H_AbstractBeamLine & , const bool)
void H_BeamParticle::computePath(const H_AbstractBeamLine * beam, const bool NonLinear) {
	TMatrixD temp_mat(MDIM,MDIM);
	double temp_x, temp_y, temp_s, temp_tx, temp_ty;

	temp_x = (positions.front())[INDEX_X];
        temp_tx = (positions.front())[INDEX_TX];
        temp_y = (positions.front())[INDEX_Y];
        temp_ty = (positions.front())[INDEX_TY];
        temp_s = (positions.front())[INDEX_S];

		double vec[MDIM] = {temp_x/URAD, tan(temp_tx/URAD), temp_y/URAD, tan(temp_ty/URAD),energy,1};

		extern bool relative_energy;
		if(relative_energy) {
        	vec[4] = energy-beam->getBeamEnergy();
		} else {
        	vec[4] = energy;
		}

	TMatrixD mat(1,MDIM,vec);

	const int N =beam->getNumberOfElements();
	double xys[LENGTH_VEC];

	double energy_loss = NonLinear ? beam->getBeamEnergy()-energy : 0;

	for (int i=0; i<N; i++) {
		const unsigned pos = i;
		if(fs < beam->getElement(pos)->getS() && fs > beam->getElement(pos-1)->getS()) {
			if(beam->getElement(pos-1)->getType()!=DRIFT) { 
				cout<<"Path starts inside element "<<beam->getElement(pos-1)->getName()<<endl;
			} else { 
				cout<<"Path starts inside unnamed drift "<<endl;
			}
			H_OpticalElement* temp_el = beam->getElement(pos-1)->clone();
			temp_el->setS(fs);
			temp_el->setLength(beam->getElement(pos)->getS() - fs);
			mat[0][0] = mat[0][0] - temp_el->getX();
			mat[0][1] = mat[0][1] - tan(temp_el->getTX());
			mat[0][2] = mat[0][2] - temp_el->getY();
			mat[0][3] = mat[0][3] - tan(temp_el->getTY());
			mat *= temp_el->getMatrix(energy_loss,mp,qp);
			mat[0][0] = mat[0][0] + temp_el->getX();
			mat[0][1] = mat[0][1] + tan(temp_el->getTX());
			mat[0][2] = mat[0][2] + temp_el->getY();
			mat[0][3] = mat[0][3] + tan(temp_el->getTY());
			xys[0] = mat.GetMatrixArray()[0]*URAD;
			xys[1] = atan(mat.GetMatrixArray()[1])*URAD;
			xys[2] = mat.GetMatrixArray()[2]*URAD;
			xys[3] = atan(mat.GetMatrixArray()[3])*URAD;
			xys[4] = temp_el->getS()+temp_el->getLength();
			addPosition(xys[0],xys[1],xys[2],xys[3],xys[4]);
			if (temp_el) delete temp_el;
		} 
		if(fs <= beam->getElement(pos)->getS()) {
			mat[0][0] = mat[0][0] - beam->getElement(pos)->getX();
			mat[0][1] = mat[0][1] - tan(beam->getElement(pos)->getTX()/URAD)*URAD;
			mat[0][2] = mat[0][2] - beam->getElement(pos)->getY();
			mat[0][3] = mat[0][3] - tan(beam->getElement(pos)->getTY()/URAD)*URAD;
			mat *= beam->getElement(pos)->getMatrix(energy_loss,mp,qp);
			mat[0][0] = mat[0][0] + beam->getElement(pos)->getX();
			mat[0][1] = mat[0][1] + tan(beam->getElement(pos)->getTX()/URAD)*URAD;
			mat[0][2] = mat[0][2] + beam->getElement(pos)->getY();
			mat[0][3] = mat[0][3] + tan(beam->getElement(pos)->getTY()/URAD)*URAD;
			xys[0] = mat.GetMatrixArray()[0]*URAD;
			xys[1] = atan(mat.GetMatrixArray()[1])*URAD;
			xys[2] = mat.GetMatrixArray()[2]*URAD;
			xys[3] = atan(mat.GetMatrixArray()[3])*URAD;
			xys[4] = beam->getElement(pos)->getS()+beam->getElement(pos)->getLength();
			addPosition(xys[0],xys[1],xys[2],xys[3],xys[4]);
		}
	}
}

// part about non-ip particle is not ready yet. use the above method in the meantime
void H_BeamParticle::computePath(const H_AbstractBeamLine & beam, const bool NonLinear) {
	TMatrixD temp_mat(MDIM,MDIM);
	double temp_x, temp_y, temp_s, temp_tx, temp_ty;

	temp_x = (positions.front())[INDEX_X];
        temp_tx = (positions.front())[INDEX_TX];
        temp_y = (positions.front())[INDEX_Y];
        temp_ty = (positions.front())[INDEX_TY];
        temp_s = (positions.front())[INDEX_S];

		double vec[MDIM] = {temp_x/URAD, tan(temp_tx/URAD), temp_y/URAD, tan(temp_ty/URAD),energy,1};

		extern bool relative_energy;
		if(relative_energy) {
        	vec[4] = energy-beam.getBeamEnergy();
		} else {
        	vec[4] = energy;
		}

	TMatrixD mat(1,MDIM,vec);

	const int N =beam.getNumberOfElements();
	double xys[LENGTH_VEC];

	double energy_loss = NonLinear ? beam.getBeamEnergy()-energy : 0;

	// modify here to allow starting at non-IP positions
	// s is distance to IP
	// initial position is already in positions vector ?
	for (int i=0; i<N; i++) {
		const unsigned pos = i;
		// if we are inside an element, we should start by adding the action
		// of the rest of this element
		if(pos > 0 && fs < beam.getElement(pos)->getS() && fs > beam.getElement(pos-1)->getS()) {
			cout<<"Path starts inside element "<<beam.getElement(pos-1)->getName()<<endl;
			H_OpticalElement* temp_el = new H_OpticalElement(*(beam.getElement(pos-1)));
			temp_el->setS(fs);
			temp_el->setLength(beam.getElement(pos)->getS() - fs);
			mat[0][0] = mat[0][0] - temp_el->getX();
			mat[0][1] = mat[0][1] - tan(temp_el->getTX());
			mat[0][2] = mat[0][2] - temp_el->getY();
			mat[0][3] = mat[0][3] - tan(temp_el->getTY());
			mat *= temp_el->getMatrix(energy_loss,mp,qp);
			mat[0][0] = mat[0][0] + temp_el->getX();
			mat[0][1] = mat[0][1] + tan(temp_el->getTX());
			mat[0][2] = mat[0][2] + temp_el->getY();
			mat[0][3] = mat[0][3] + tan(temp_el->getTY());
		} else if(fs >= beam.getElement(pos)->getS()) {
			mat[0][0] = mat[0][0] - beam.getElement(pos)->getX();
			mat[0][1] = mat[0][1] - tan(beam.getElement(pos)->getTX());
			mat[0][2] = mat[0][2] - beam.getElement(pos)->getY();
			mat[0][3] = mat[0][3] - tan(beam.getElement(pos)->getTY());
			mat *= beam.getElement(pos)->getMatrix(energy_loss,mp,qp);
			mat[0][0] = mat[0][0] + beam.getElement(pos)->getX();
			mat[0][1] = mat[0][1] + tan(beam.getElement(pos)->getTX());
			mat[0][2] = mat[0][2] + beam.getElement(pos)->getY();
			mat[0][3] = mat[0][3] + tan(beam.getElement(pos)->getTY());
		}
		xys[0] = mat.GetMatrixArray()[0]*URAD;
		xys[1] = atan(mat.GetMatrixArray()[1])*URAD;
		xys[2] = mat.GetMatrixArray()[2]*URAD;
		xys[3] = atan(mat.GetMatrixArray()[3])*URAD;
		xys[4] = beam.getElement(pos)->getS()+beam.getElement(pos)->getLength();
		addPosition(xys[0],xys[1],xys[2],xys[3],xys[4]);
		fx = xys[0];
		fy = xys[2];
		thx = xys[1];
		thy = xys[3];
	}
}

void H_BeamParticle::resetPath() {
	double temp_x, temp_y, temp_s, temp_tx, temp_ty;

	temp_x = (positions.front())[INDEX_X];
	temp_tx = (positions.front())[INDEX_TX];
	temp_y = (positions.front())[INDEX_Y];
	temp_ty = (positions.front())[INDEX_TY];
	temp_s = (positions.front())[INDEX_S];
	positions.clear();
	addPosition(temp_x,temp_tx,temp_y,temp_ty,temp_s);
}

const TVectorD * H_BeamParticle::getPosition(const int element_position) const {
 	const int N = (element_position<0)?0:(( ((unsigned int) element_position)>positions.size()-1)?positions.size()-1:element_position);
 	return &(*(positions.begin()+N));// same as "return &positions[N];", but more efficient
}
